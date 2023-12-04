/*
 * Copyright (C) 2015-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib.h>

#include "utils-linux.h"


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
_get_user_sid (void)
{
    const char *cmd = "whoami.exe /user /fo csv";
    char *cmd_out = NULL, *cmd_err = NULL, *result = NULL;
    int exit_code;
    GError *error = NULL;

    if (FALSE == g_spawn_command_line_sync(
        cmd, &cmd_out, &cmd_err, &exit_code, &error))
    {
        g_debug ("Error running whoami.exe: %s", error->message);
        goto sid_cleanup;
    }

    /* Likely whoami.exe from MSYS2, cygwin, etc */
    if (exit_code != 0)
    {
        g_debug ("Not expected whoami.exe, error is: %s", cmd_err);
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
            g_critical ("Incorrect SID format '%s'", result);
            g_free (result);
            result = NULL;
        }
    }

  sid_cleanup:
    g_free (cmd_out);
    g_free (cmd_err);
    g_clear_error (&error);
    return result;
}


static GSList *
_probe_mounts (void)
{
    GSList *result = NULL;
    GError *error = NULL;
    gsize len;
    char *mounts_data = NULL;
    const char *fstype = "9p";

    if (! g_file_get_contents ("/proc/self/mounts",
        &mounts_data, &len, &error))
    {
        g_critical ("Fail reading mount data: %s", error->message);
        g_clear_error (&error);
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
enumerate_drive_bins (void)
{
    GSList *result = NULL;
    char *sid = _get_user_sid ();

    if (sid == NULL)
        return NULL;

    result = _probe_mounts ();
    if (result == NULL)
    {
        g_critical ("%s", "Failed to detect valid Windows drive");
        return NULL;
    }

    for (GSList *ptr = result; ptr != NULL; ptr = ptr->next)
    {
        char *old = (char *) ptr->data;
        ptr->data = g_build_filename (old, "$Recycle.bin", sid, NULL);
        g_free (old);
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
