/*
 * Copyright (C) 2023-2024, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "utils-io.h"
#include "utils-platform.h"


static FILE        *out_fh             = NULL;
static FILE        *err_fh             = NULL;
static FILE        *prev_fh            = NULL;
static char        *tmpfile_path       = NULL;


static void
_local_print   (const char   *str,
                bool          is_stdout)
{
    FILE  *fh;

    if (!g_utf8_validate (str, -1, NULL)) {
        g_critical (_("String not in UTF-8 encoding: %s"), str);
        return;
    }

    fh = is_stdout ? out_fh : err_fh;

#ifdef G_OS_WIN32
    if (fh == NULL)
    {
        wchar_t *wstr = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);
        puts_wincon (is_stdout, wstr);
        g_free (wstr);
    }
    else
#endif
        fputs (str, fh);
}


static void
_local_printout   (const char   *str)
{
    _local_print (str, true);
}


static void
_local_printerr   (const char   *str)
{
    _local_print (str, false);
}


/**
 * @brief Wrapper of `g_mkstemp()` that manages file handle and
 * output behind the scene
 * @param error Location of `GError` pointer to store potential problem
 * @return `true` if temp file is created successfully. Upon problem,
 * returns `false` and `error` is set.
 */
bool
get_tempfile    (GError   **error)
{
    int     fd, e = 0;
    FILE   *tmp_fh;

    // segfaults if string is pre-allocated in stack
    tmpfile_path = g_strdup ("rifiuti-XXXXXX");

    if (-1 == (fd = g_mkstemp(tmpfile_path))) {
        e = errno;
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
            _("Can not create temp file: %s"), g_strerror(e));
        return false;
    }

    if (NULL == (tmp_fh = fdopen (fd, "wb"))) {
        e = errno;
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
            _("Can not open temp file: %s"), g_strerror(e));
        g_close (fd, NULL);
        return false;
    }

    prev_fh = out_fh;
    out_fh  = tmp_fh;

    return true;
}


bool
clean_tempfile   (char      *dest,
                  GError   **error)
{
    int result;

    if (tmpfile_path == NULL)
        return true;

    if (prev_fh)
    {
        fclose (out_fh);
        out_fh = prev_fh;
    }

    if (0 != (result = g_rename (tmpfile_path, dest)))
    {
        int e = errno;
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
            _("%s. Temp file '%s' can't be moved to destination."),
            g_strerror(e), tmpfile_path);
    }
    g_free (tmpfile_path);

    return (result == 0);
}


void
init_handles   (void)
{
#ifdef G_OS_WIN32
    if (! init_wincon_handle (false))
#endif
        err_fh = stderr;
    g_set_printerr_handler (_local_printerr);

#ifdef G_OS_WIN32
    if (! init_wincon_handle (true))
#endif
        out_fh = stdout;
    g_set_print_handler (_local_printout);
}


/**
 * @brief Close all output / error file handles before exit
 */
void
close_handles   (void)
{
    if (out_fh != NULL) fclose (out_fh);
    if (err_fh != NULL) fclose (err_fh);
    return;
}


