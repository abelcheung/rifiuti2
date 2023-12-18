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
 * @return Desired search result in `GPtrArray`
 * @note This routine does not require all lines to have same
 * separator; result could be extracted as long as the
 * particular matching line has all the right conditions.
 */
static GPtrArray *
_search_delimited_text (const char   *haystack,
                        const char   *needle,
                        const char   *sep,
                        gsize         needle_pos,
                        gsize         result_pos)
{
    GPtrArray *result = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

    char **lines = g_strsplit (haystack, "\n", 0);
    for (gsize i = 0; i < g_strv_length (lines); i++)
    {
        if (NULL == g_strstr_len (lines[i], -1, needle))
            continue;
        g_debug ("Found potential match '%s' in line '%s'", needle, lines[i]);
        char **fields = g_strsplit (g_strchomp (lines[i]), sep, 0);
        gsize nfields = g_strv_length (fields);
        if (nfields > needle_pos &&
            nfields > result_pos &&
            g_strcmp0 (fields[needle_pos], needle) == 0)
        {
            g_ptr_array_add (result, g_strdup (fields[result_pos]));
        }
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
        if (g_utf8_validate (cmd_err, 10, NULL))
            g_set_error (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
                "Error running whoami: %s", cmd_err);
        else
            // When running under valgrind, the stderr is set to
            // contain binary data of whoami.exe
            g_set_error_literal (error, R2_MISC_ERROR, R2_MISC_ERROR_GET_SID,
                "Error running whoami with unknown reason");
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
    g_clear_error (&cmd_error);
    return result;
}


/**
 * @brief Check mount points for potential Windows drive
 * @param error Location to store `GError` upon problem
 * @return `GPtrArray` containing found mount points,
 * or `NULL` if problem arises
 */
static GPtrArray *
_probe_mounts   (GError   **error)
{
    GPtrArray *result;
    GError *read_error = NULL;
    gsize len;
    char *mounts_data = NULL;
    const char *fstype = "9p";
    const char *proc = "/proc/self/mounts";

    if (! g_file_get_contents (proc, &mounts_data, &len, &read_error))
    {
        g_set_error_literal (error, R2_MISC_ERROR,
            R2_MISC_ERROR_ENUMERATE_MNT, read_error->message);
        g_clear_error (&read_error);
        return NULL;
    }

    result = _search_delimited_text (
        (const char *) mounts_data, fstype, " ", 2, 1);
    g_free (mounts_data);
    return result;
}


/**
 * @brief Probe for possible Windows Recycle Bin under WSL Linux
 * @param error Location to store `GError` when problem arises
 * @return List of possible Windows paths in `GPtrArray`
 */
GPtrArray *
enumerate_drive_bins   (GError   **error)
{
    GPtrArray *mnt_pts, *result;
    char *sid;

    if (NULL == (sid = _get_user_sid (error)))
        return NULL;

    mnt_pts = _probe_mounts (error);
    if (mnt_pts == NULL)
        return NULL;
    if (mnt_pts->len == 0)
    {
        g_ptr_array_free (mnt_pts, TRUE);
        return NULL;
    }

    result = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

    for (gsize i = 0; i < mnt_pts->len; i++)
    {
        char *full_rbin_path = g_build_filename (
            (char *) (mnt_pts->pdata[i]), "$Recycle.bin", sid, NULL);
        if (g_file_test (full_rbin_path, G_FILE_TEST_EXISTS))
            g_ptr_array_add (result, full_rbin_path);
        else
            g_free (full_rbin_path);
    }
    g_ptr_array_free (mnt_pts, TRUE);

    if (result->len == 0) {
        g_set_error_literal (error, R2_MISC_ERROR,
            R2_MISC_ERROR_ENUMERATE_MNT,
            "No recycle bin found on system");
        g_ptr_array_free (result, TRUE);
        result = NULL;
    }
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
    GPtrArray *search_result;

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

    search_result = _search_delimited_text (
        (const char *)cmd_out, "ProductName", "    ", 1, 3);
    g_assert (search_result->len == 1);
    result = g_strdup ((char *) (search_result->pdata[0]));
    g_ptr_array_free (search_result, TRUE);

    prod_cleanup:

    g_free (cmd_out);
    g_free (cmd_err);
    g_clear_error (&error);
    return result;
}
