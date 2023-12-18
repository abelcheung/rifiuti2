/*
 * Copyright (C) 2015-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <stdbool.h>
#include <lmcons.h>
#include <aclapi.h>
#include <authz.h>
#include <sddl.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "utils-error.h"
#include "utils-platform.h"

static HANDLE wincon_fh = NULL;
static HANDLE winerr_fh = NULL;
static PSID   sid       = NULL;


G_DEFINE_QUARK (rifiuti-misc-error-quark, rifiuti_misc_error)


/* GUI message box */
void
gui_message (const char *message)
{
    wchar_t *title = L"This is a command line application";
    wchar_t *body = (wchar_t *) g_utf8_to_utf16 (
        message, -1, NULL, NULL, NULL);

    if (body)
    {
        MessageBoxW (NULL, body, title,
            MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        g_free (body);
        return;
    }

    body = L"(Failure to display help)";
    MessageBoxW (NULL, body, title,
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    return;
}


/*!
 * `strftime()` on Windows can show garbage timezone name, because its
 * encoding does not match console codepage. For example, strftime %Z result
 * is in CP936 encoding for zh-HK, while console codepage is CP950.
 *
 * OTOH, `wcsftime() %Z` would not return anything if console codepage is
 * set to any non-default codepage for current system.
 *
 * `GetTimeZoneInformation()` returns sensible result regardless of
 * console codepage setting.
 */
char *
get_win_timezone_name (void)
{
    TIME_ZONE_INFORMATION tzinfo;
    WCHAR    *name    = NULL;
    DWORD     id;
    char     *result  = NULL;
    GError   *error   = NULL;

    id = GetTimeZoneInformation (&tzinfo);
    g_debug ("%s(): GetTimeZoneInformation() = %lu", __func__, id);

    switch (id)
    {
        case TIME_ZONE_ID_UNKNOWN:
        case TIME_ZONE_ID_STANDARD:
            name = tzinfo.StandardName;
            break;
        case TIME_ZONE_ID_DAYLIGHT:
            name = tzinfo.DaylightName;
            break;
        default:
            {
                char *msg = g_win32_error_message (GetLastError ());
                g_critical ("%s(): %s", __func__, msg);
                g_free (msg);
            }
            break;
    }

    if (name) {
        result = g_utf16_to_utf8 ((const gunichar2 *) name,
            -1, NULL, NULL, &error);
        if (error) {
            g_critical ("%s(): %s", __func__, error->message);
            g_clear_error (&error);
        }
    }

    if (result)
        return result;
    else
        return g_strdup (_("(Failed to retrieve timezone name)"));
}


/**
 * @brief Get SID of current user
 * @return Pointer to SID structure, or `NULL` upon failure
 * @note Code is derived from example of
 * [GetEffectiveRightsFromAcl()](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-geteffectiverightsfromacla),
 */
static PSID
_get_user_sid   (GError   **error)
{
    static bool       tried      = false;
    wchar_t           username[UNLEN + 1],
                     *domainname = NULL;
    char             *errmsg = NULL;
    DWORD             e          = 0,
                      bufsize    = UNLEN + 1,
                      sidsize    = 0,
                      domainsize = 0;
    SID_NAME_USE      sidtype;

    if (tried) return sid;
    tried = true;

    if (! GetUserNameW (username, &bufsize))
    {
        errmsg = g_win32_error_message (GetLastError());
        g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
            "GetUserName() failure: %s", errmsg);
        g_free (errmsg);
        return NULL;
    }

    if (! LookupAccountNameW (NULL, username, NULL, &sidsize,
            NULL, &domainsize, &sidtype))
    {
        e = GetLastError();
        if ( e != ERROR_INSUFFICIENT_BUFFER )
        {
            errmsg = g_win32_error_message (e);
            g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
                "LookupAccountName() failure (1): %s", errmsg);
            g_free (errmsg);
            return NULL;
        }
    }

    // XXX Don't use standard Windows way (AllocateAndInitializeSid).
    // Random unpredictable test failures would result, even when
    // tests are run serially.
    sid = g_malloc (sidsize);

    // Unused but still needed, otherwise LookupAccountName call
    // would fail
    domainname = g_malloc (domainsize * sizeof (wchar_t));

    if (LookupAccountNameW (NULL, username, sid, &sidsize,
            domainname, &domainsize, &sidtype))
    {
        g_free (domainname);
        return sid;
    }

    errmsg = g_win32_error_message (GetLastError());
    g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
        "LookupAccountName() failure (2): %s", errmsg);
    g_free (errmsg);

    g_free (sid);
    g_free (domainname);

    return NULL;
}


/**
 * @brief Probe for possible Windows Recycle Bin paths
 * @param error Location to store `GError` when problem arises
 * @return List of possible Windows paths in `GPtrArray`
 */
GPtrArray *
enumerate_drive_bins   (GError   **error)
{
    DWORD         drive_bitmap;
    PSID          sid = NULL;
    char         *errmsg = NULL, *sid_str = NULL;
    static char   drive_root[4] = "A:\\";
    GPtrArray    *result = NULL;

    if (NULL == (sid = _get_user_sid (error)))
        return NULL;

    if (! ConvertSidToStringSidA(sid, &sid_str)) {
        errmsg = g_win32_error_message (GetLastError());
        g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
            "ConvertSidToStringSidA() failure: %s", errmsg);
        goto enumerate_cleanup;
    }

    if (! (drive_bitmap = GetLogicalDrives())) {
        errmsg = g_win32_error_message (GetLastError());
        g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_ENUMERATE_MNT,
            "GetLogicalDrives() failure: %s", errmsg);
        goto enumerate_cleanup;
    }

    result = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

    for (gsize i = 0; i < sizeof(DWORD) * CHAR_BIT; i++)
    {
        if (! (drive_bitmap & (1 << i)))
            continue;
        drive_root[0] = 'A' + i;
        UINT type = GetDriveTypeA(drive_root);
        if (   (type == DRIVE_NO_ROOT_DIR)
            || (type == DRIVE_UNKNOWN    )
            || (type == DRIVE_REMOTE     )
            || (type == DRIVE_CDROM      )) {
            g_debug ("%s unwanted, type = %u", drive_root, type);
            continue;
        }
        char *full_rbin_path = g_build_filename (drive_root,
            "$Recycle.bin", sid_str, NULL);
        if (g_file_test (full_rbin_path, G_FILE_TEST_EXISTS))
            g_ptr_array_add (result, full_rbin_path);
        else
            g_free (full_rbin_path);
    }

    // Might be possible that even C:\ is network drive
    // (e.g. thin client), but still report that as
    // error for live mode
    if (result->len == 0) {
        g_set_error_literal (error, R2_MISC_ERROR,
            R2_MISC_ERROR_ENUMERATE_MNT,
            _("No recycle bin found on system"));
        g_ptr_array_free (result, TRUE);
        result = NULL;
    }

    enumerate_cleanup:

    if (sid_str != NULL)
        LocalFree (sid_str);
    g_free (errmsg);
    return result;
}

/*
 * Get Windows product name via registry
 */
char *
windows_product_name (void)
{
    LSTATUS    status;
    DWORD      str_size;
    gunichar2 *buf;
    gunichar2 *subkey = L"software\\microsoft\\windows nt\\currentversion";
    gunichar2 *keyvalue  = L"ProductName";
    char      *result;

    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        subkey,
        keyvalue,
        RRF_RT_REG_SZ,
        NULL,
        NULL,
        &str_size
    );

    g_debug ("1st RegGetValueW(ProductName): status = %li, str_size = %lu",
        status, str_size);

    if (status != ERROR_SUCCESS)
        return NULL;

    buf = g_malloc ((gsize) str_size);
    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        subkey,
        keyvalue,
        RRF_RT_REG_SZ,
        NULL,
        buf,
        &str_size
    );

    g_debug ("2nd RegGetValueW(ProductName): status = %li", status);

    if (status != ERROR_SUCCESS) {
        g_free (buf);
        return NULL;
    }

    result = g_utf16_to_utf8(buf, -1, NULL, NULL, NULL);
    g_free (buf);
    return result;
}

/*!
 * Fetch ACL access mask using Authz API
 */
gboolean
can_list_win32_folder (const char   *path,
                       GError      **error)
{
    char                  *errmsg = NULL;
    gunichar2             *wpath;
    gboolean               ret = FALSE;
    PSID                   sid;
    DWORD                  dw, dw2;
    PSECURITY_DESCRIPTOR   sec_desc;
    ACCESS_MASK            mask;
    AUTHZ_RESOURCE_MANAGER_HANDLE authz_manager;
    AUTHZ_CLIENT_CONTEXT_HANDLE   authz_ctxt = NULL;
    AUTHZ_ACCESS_REQUEST          authz_req = { MAXIMUM_ALLOWED, NULL, NULL, 0, NULL };
    AUTHZ_ACCESS_REPLY            authz_reply;

    if (NULL == (sid = _get_user_sid (error)))
        return FALSE;

    wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
    if (wpath == NULL)
        return FALSE;

    if ( !AuthzInitializeResourceManager (AUTHZ_RM_FLAG_NO_AUDIT,
                NULL, NULL, NULL, NULL, &authz_manager) )
    {
        errmsg = g_win32_error_message (GetLastError());
        g_printerr (_("AuthzInitializeResourceManager() failed: %s"), errmsg);
        g_printerr ("\n");
        goto traverse_fail;
    }

    dw = GetNamedSecurityInfoW ((wchar_t *)wpath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION |
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
            NULL, NULL, NULL, NULL, &sec_desc);
    if ( dw != ERROR_SUCCESS )
    {
        errmsg = g_win32_error_message (dw);
        g_printerr (_("Failed to retrieve Discretionary ACL info for '%s': %s"), path, errmsg);
        g_printerr ("\n");
        goto traverse_getacl_fail;
    }

    if ( !AuthzInitializeContextFromSid (0, sid, authz_manager,
                NULL, (LUID) {0} /* unused */, NULL, &authz_ctxt) )
    {
        errmsg = g_win32_error_message (GetLastError());
        g_printerr (_("AuthzInitializeContextFromSid() failed: %s"), errmsg);
        g_printerr ("\n");
        goto traverse_getacl_fail;
    }

    authz_reply = (AUTHZ_ACCESS_REPLY) { 1, &mask, &dw2, &dw }; /* last 2 param unused */
    if ( !AuthzAccessCheck (0, authz_ctxt, &authz_req, NULL, sec_desc,
                NULL, 0, &authz_reply, NULL ) )
    {
        errmsg = g_win32_error_message (GetLastError());
        g_printerr (_("AuthzAccessCheck() failed: %s"), errmsg);
        g_printerr ("\n");
    }
    else
    {
        /*
         * We only need permission to list directory; even directory traversal is
         * not needed, because we are going to access the files directly later.
         * Unlike Unix, no read permission on parent folder is needed to list
         * files within.
         */
        if ( (mask & FILE_LIST_DIRECTORY) == FILE_LIST_DIRECTORY &&
                (mask & FILE_READ_EA) == FILE_READ_EA )
            ret = TRUE;
        else {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_ACCES,
                _("Error listing dir '%s': disallowed under Windows ACL."), path);
        }

        g_debug ("Access Mask hex for '%s': 0x%lX", path, mask);
    }

    AuthzFreeContext (authz_ctxt);

  traverse_getacl_fail:
    LocalFree (sec_desc);
    AuthzFreeResourceManager (authz_manager);

  traverse_fail:
    g_free (errmsg);
    g_free (wpath);
    return ret;
}

/*!
 * Initialize console handle under Windows
 *
 * Used only when output is Windows native console. For all other cases
 * unix-style file stream is used.
 */
gboolean
init_wincon_handle (gboolean is_stdout)
{
    HANDLE h;
    DWORD console_type;

    if (is_stdout)
        h = GetStdHandle (STD_OUTPUT_HANDLE);
    else
        h = GetStdHandle (STD_ERROR_HANDLE);
    console_type = GetFileType (h);

    /*
     * FILE_TYPE_CHAR only happens when output is a native Windows
     * command prompt or powershell 5.x. This is true even for latest
     * Windows 10 / Server, despite console revamp since 1809. Those
     * need to be dealt with using wide char API.
     *
     * For most 3rd party terminals and for output redirection (file
     * or pipe), GetFileType() would return FILE_TYPE_PIPE, which
     * handles UTF-8 properly. Powershell core also supports UTF-8 by
     * default.
     */
    switch (console_type) {
        case FILE_TYPE_CHAR:
            g_debug ("GetFileType() = %s", "FILE_TYPE_CHAR");
            if (is_stdout)
                wincon_fh = h;
            else
                winerr_fh = h;
            return TRUE;
        case FILE_TYPE_PIPE:
            g_debug ("GetFileType() = %s", "FILE_TYPE_PIPE");
            return FALSE;
        default:
            g_debug ("GetFileType() = %lu", console_type);
            return FALSE;
    }
}


void
puts_wincon (gboolean       is_stdout,
             const wchar_t *wstr)
{
    HANDLE h = is_stdout ? wincon_fh : winerr_fh;

    g_return_if_fail (wstr != NULL);
    g_return_if_fail (h    != NULL);

    WriteConsoleW (h, wstr, wcslen (wstr), NULL, NULL);
}


static void
_close_wincon_handles (void)
{
    if (wincon_fh != NULL)
        CloseHandle (wincon_fh);
    if (winerr_fh != NULL)
        CloseHandle (winerr_fh);
    return;
}


void
cleanup_windows_res (void)
{
    _close_wincon_handles ();
    g_free (sid);
}
