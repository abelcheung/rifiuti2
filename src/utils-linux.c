/*
 * Copyright (C) 2015-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include "utils-conv.h"
#include "utils-error.h"
#include "utils-platform.h"


G_DEFINE_QUARK (rifiuti-misc-error-quark, rifiuti_misc_error)


/**
 * @brief The result of not wishing to use GIOChannel
 * @param haystack Content to be search
 * @param needle Substring to look for
 * @param separator Field separator in desired line
 * @param needle_pos Field number where needle is supposed to be
 * @param result_pos Field number of data we want
 * @return GSList of desired data
 * @note This routine does not require all lines to have same separator; result could be extracted as long as the particular matching line has all the right conditions.
 */
static GSList *
_search_delimited_text (const char *haystack,
                        const char *needle,
                        const char *sep,
                        gsize needle_pos,
                        gsize result_pos)
{
    GSList *result = NULL;
    char **lines = g_strsplit (haystack, "\n", 0);
    for (gsize i = 0; i < g_strv_length (lines); i++)
    {
        if (NULL == g_strstr_len (lines[i], -1, needle))
            continue;
        g_debug ("Found potential match '%s' in line '%s'", needle, lines[i]);
        char **fields = g_strsplit (g_strchomp (lines[i]), sep, 0);
        gsize nfields = g_strv_length (fields);
        if (nfields >= needle_pos && nfields > result_pos)
            if (g_strcmp0 (fields[needle_pos], needle) == 0)
                result = g_slist_prepend (result, g_strdup (fields[result_pos]));
        g_strfreev (fields);
    }

    g_strfreev (lines);
    return result;
}


/**
 * @brief Probe Windows SID from inside WSL Linux
 * @return SID string "S-1-...", or `NULL` if failure
 */
static char *
_get_user_sid   (GError   **error)
{
    const char *cmd = "whoami.exe /user /fo csv";
    char *cmd_out = NULL, *cmd_err = NULL, *result = NULL;
    int exit_code;
    GError *cmd_error = NULL;

    if (FALSE == g_spawn_command_line_sync(
        cmd, &cmd_out, &cmd_err, &exit_code, &cmd_error))
    {
        g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
            "Error running whoami: %s", cmd_error->message);
        goto sid_cleanup;
    }
    else if (exit_code != 0)  // e.g. whoami.exe from MSYS2
    {
        g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
            "Error running whoami: %s", cmd_err);
        goto sid_cleanup;
    }

    g_debug ("whoami output: %s", cmd_out);

    // No full fledged CSV parsing. Sample output:
    // "User Name","SID"
    // "machine\user","S-1-5-21-..."
    {
        char **lines = g_strsplit (cmd_out, "\r\n", 0);
        char **fields = g_strsplit (lines[1], ",", 0);
        glong len = g_utf8_strlen (fields[1], -1);
        result = g_utf8_substring (fields[1], 1, len - 1);
        g_strfreev (fields);
        g_strfreev (lines);

        if (result[0] != 'S') {
            g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
                "Invalid format '%s'", result);
            g_free (result);
            result = NULL;
        }
    }

    sid_cleanup:

    g_free (cmd_out);
    g_free (cmd_err);
    g_error_free (cmd_error);
    return result;
}


static GSList *
_probe_mounts   (GError   **error)
{
    GSList *result = NULL;
    GError *read_error = NULL;
    gsize len;
    char *mounts_data = NULL;
    const char *fstype = "9p";
    const char *proc = "/proc/self/mounts";

    if (! g_file_get_contents (proc, &mounts_data, &len, &read_error))
    {
        g_set_error_literal (error, R2_MISC_ERROR,
            R2_MISC_ERROR_ENUMERATE_MNT, read_error->message);
        g_error_free (read_error);
        return NULL;
    }

    result = _search_delimited_text (
        (const char *)mounts_data, fstype, " ", 2, 1);
    g_free (mounts_data);
    return result;
}


/**
 * @brief Probe for possible Windows Recycle Bin under WSL Linux
 * @return List of possible Windows paths to be checked
 */
GSList *
enumerate_drive_bins   (GError   **error)
{
    GSList *result = NULL, *mnt_pts;
    char *sid;

    if (NULL == (sid = _get_user_sid (error)))
        return NULL;

    if (NULL == (mnt_pts = _probe_mounts (error)))
        return NULL;

    for (GSList *ptr = mnt_pts; ptr != NULL; ptr = ptr->next)
    {
        char *full_rbin_path = g_build_filename (
            (char *) ptr->data, "$Recycle.bin", sid, NULL);
        if (g_file_test (full_rbin_path, G_FILE_TEST_EXISTS))
            result = g_slist_prepend (result, full_rbin_path);
        else
            g_free (full_rbin_path);
    }

    if (result == NULL)
        g_set_error_literal (error, R2_MISC_ERROR,
            R2_MISC_ERROR_ENUMERATE_MNT,
            "No recycle bin found on system");
    g_slist_free_full (mnt_pts, (GDestroyNotify) g_free);

    return result;
}


/**
 * @brief Get Windows product name via registry
 * @return Windows product name as ASCII string
 */
char *
windows_product_name (void)
{
    const char *cmd = "reg.exe query \"HKLM\\software\\microsoft\\windows nt\\currentversion\" /v ProductName";
    char *cmd_out = NULL, *cmd_err = NULL, *result = NULL;
    int exit_code;
    GError *error = NULL;

    if (FALSE == g_spawn_command_line_sync(
            cmd, &cmd_out, &cmd_err, &exit_code, &error))
    {
        g_debug ("Error running reg.exe: %s", error->message);
        goto prod_cleanup;
    }

    if (exit_code != 0)
    {
        g_debug ("reg.exe error: %s", cmd_err);
        goto prod_cleanup;
    }

    g_debug ("reg.exe output: %s", cmd_out);

    GSList *searches = _search_delimited_text (
        (const char *)cmd_out, "ProductName", "    ", 1, 3);
    g_assert (searches != NULL && searches->next == NULL);
    result = searches->data;
    g_slist_free_1 (searches);

  prod_cleanup:
    g_free (cmd_out);
    g_free (cmd_err);
    g_clear_error (&error);
    return result;
}
