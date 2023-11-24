/*
 * Copyright (C) 2015-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include "config.h"
#include "utils-win.h"

#include <lmcons.h>
#include <windows.h>
#include <aclapi.h>
#include <authz.h>
#include <sddl.h>

#include <glib.h>
#include <glib/gi18n.h>


static HANDLE wincon_fh = NULL;
static HANDLE winerr_fh = NULL;


/* GUI message box */
void
gui_message (const char *message)
{
    gunichar2 *title, *body;
    GError *error = NULL;

    title = g_utf8_to_utf16 (_("This is a command line application"),
        -1, NULL, NULL, &error);
    if (error) {
        g_clear_error (&error);
        title = g_utf8_to_utf16 ("This is a command line application",
            -1, NULL, NULL, NULL);
    }

    body = g_utf8_to_utf16 (message, -1, NULL, NULL, &error);
    if (error) {
        g_clear_error (&error);
        body = g_utf8_to_utf16 ("(Original message failed to be displayed in UTF-16)",
            -1, NULL, NULL, NULL);
    }

    /* Takes advantage of the fact that LPCWSTR (wchar_t) is actually 16bit on Windows */
    MessageBoxW (NULL, (LPCWSTR) body, (LPCWSTR) title,
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    g_free (title);
    g_free (body);
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
    wchar_t  *name;
    DWORD     id;
    char     *ret;
    GError   *err = NULL;

    id = GetTimeZoneInformation (&tzinfo);

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
            ret = g_win32_error_message (GetLastError ());
            g_critical ("%s", ret);
            g_free (ret);
            return g_strdup (_("(Failed to retrieve timezone name)"));
            break;
    }

    ret = g_utf16_to_utf8 ( (const gunichar2 *) name, -1, NULL, NULL, &err);
    if (err == NULL)
        return ret;

    g_warning ("%s", err->message);
    g_error_free (err);
    return NULL;
}


/*!
 * Retrieve current user name and convert it to SID
 *
 * Following functions originates from [example of `GetEffectiveRightsFromAcl()`][1],
 * which is not about the function itself but a _replacement_ of it (shrug).
 *
 * [1]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa446637(v=vs.85).aspx
 */
static PSID
_get_user_sid (void)
{
    gboolean       ok;
    gunichar2      username[UNLEN + 1], *domainname;
    char           *errmsg;
    DWORD          err = 0, bufsize = UNLEN + 1, sidsize = 0, domainsize = 0;
    PSID           sid;
    SID_NAME_USE   sidtype;

    if ( !GetUserNameW (username, &bufsize) )
    {
        errmsg = g_win32_error_message (GetLastError());
        g_critical (_("Failed to get current user name: %s"), errmsg);
        goto getsid_fail;
    }

    ok = LookupAccountNameW (NULL, username, NULL, &sidsize,
            NULL, &domainsize, &sidtype);
    if (!ok) {
        err = GetLastError();
        if ( err != ERROR_INSUFFICIENT_BUFFER )
        {
            errmsg = g_win32_error_message (err);
            g_critical (_("1st LookupAccountName() failed: %s"), errmsg);
            goto getsid_fail;
        }
    }

    sid = g_malloc (sidsize);
    domainname = g_malloc (domainsize * sizeof(gunichar2));

    ok = LookupAccountNameW (NULL, username, sid, &sidsize,
            domainname, &domainsize, &sidtype);
    g_free (domainname);  // unused
    if (ok)
        return sid;

    errmsg = g_win32_error_message (GetLastError());
    g_critical (_("2nd LookupAccountName() failed: %s"), errmsg);
    g_free (sid);

  getsid_fail:
    g_free (errmsg);
    return NULL;
}

/*
 * Probe for logical drives on Windows and return their
 * corresponding recycle bin paths for current user
 */
GSList *
enumerate_drive_bins (void)
{
    DWORD         drive_bitmap;
    PSID          sid = NULL;
    char         *errmsg = NULL, *sid_str = NULL;
    static char   drive_root[4] = "A:\\";
    GSList       *result = NULL;

    if (! (drive_bitmap = GetLogicalDrives())) {
        errmsg = g_win32_error_message (GetLastError());
        g_critical (_("Failed to enumerate drives in system: %s"), errmsg);
        goto enumerate_cleanup;
    }

    if (NULL == (sid = _get_user_sid())) {
        g_critical (_("Failed to get SID of current user"));
        goto enumerate_cleanup;
    }
    if (! ConvertSidToStringSidA(sid, &sid_str)) {
        errmsg = g_win32_error_message (GetLastError());
        g_critical (_("Failed to convert SID to string: %s"), errmsg);
        goto enumerate_cleanup;
    }

    for (int i = 0; i < sizeof(DWORD) * CHAR_BIT; i++) {
        if (! (drive_bitmap & (1 << i)))
            continue;
        drive_root[0] = 'A' + i;
        UINT type = GetDriveTypeA(drive_root);
        if (   (type == DRIVE_NO_ROOT_DIR)
            || (type == DRIVE_UNKNOWN    )
            || (type == DRIVE_REMOTE     )
            || (type == DRIVE_CDROM      )) {
            g_debug ("%s is not a logical drive we want, skipped", drive_root);
            continue;
        }
        char *path = g_strdup_printf ("%s$Recycle.Bin\\%s",
            drive_root, sid_str);

        result = g_slist_append (result, path);
    }

    enumerate_cleanup:
    // sid_str owned by system
    g_free (sid);
    g_free (errmsg);
    return result;
}

/*
 * Get Windows product name via registry
 */
gunichar2 *
windows_product_name (void)
{
    LSTATUS    status;
    DWORD      str_size;
    gunichar2 *product_name;
    gunichar2 *subkey = L"software\\microsoft\\windows nt\\currentversion";
    gunichar2 *keyvalue  = L"ProductName";

    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        subkey,
        keyvalue,
        RRF_RT_REG_SZ,
        NULL,
        NULL,
        &str_size
    );

    if ((status != ERROR_SUCCESS) || (str_size > G_MAXSIZE))
        return NULL;

    product_name = g_malloc ((gsize) str_size);
    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        subkey,
        keyvalue,
        RRF_RT_REG_SZ,
        NULL,
        product_name,
        &str_size
    );

    return (status == ERROR_SUCCESS) ? product_name : NULL;
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

    if ( NULL == ( sid = _get_user_sid() ) )
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

        /* use glib type to avoid including more header */
        g_debug ("Access Mask hex for '%s': 0x%X", path, (guint32) mask);
    }

    AuthzFreeContext (authz_ctxt);

  traverse_getacl_fail:
    LocalFree (sec_desc);
    AuthzFreeResourceManager (authz_manager);

  traverse_fail:
    g_free (sid);
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
close_wincon_handle (void)
{
    if (wincon_fh != NULL)
        CloseHandle (wincon_fh);
    return;
}

void
close_winerr_handle (void)
{
    if (winerr_fh != NULL)
        CloseHandle (winerr_fh);
    return;
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
