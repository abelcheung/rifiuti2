/*
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include "config.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include "utils.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  include <sys/timeb.h>
#  include "utils-win.h"
#endif

/* These aren't intended for public */
#define DECL_OPT_CALLBACK(func)          \
gboolean func (const gchar   *opt_name,  \
               const gchar   *value,     \
               gpointer       data,      \
               GError       **err)

static DECL_OPT_CALLBACK(_check_legacy_encoding);
static DECL_OPT_CALLBACK(_set_output_path);
static DECL_OPT_CALLBACK(_option_deprecated);
static DECL_OPT_CALLBACK(_set_opt_delim);
static DECL_OPT_CALLBACK(_set_opt_noheading);
static DECL_OPT_CALLBACK(_set_output_xml);


/**
 * @brief More detailed OS version guess from artifacts
 * @note This is different from `detected_os_ver`, which only checks for
 * first few bytes. It is a more detailed breakdown, and for detection of
 * exact Windows version from various recycle bin artifacts.
 * @warning MUST match order of `os_strings` array except
 *          the `UNKNOWN` entry
 */
typedef enum
{
    OS_GUESS_UNKNOWN = -1,
    OS_GUESS_95,
    OS_GUESS_NT4,
    OS_GUESS_98,
    OS_GUESS_ME,
    OS_GUESS_2K,
    OS_GUESS_XP_03,
    OS_GUESS_2K_03,   /* Empty recycle bin, full detection impossible */
    OS_GUESS_VISTA,   /* includes everything up to 8.1 */
    OS_GUESS_10
} _os_guess;

/**
 * @brief Outputed string for OS detection from artifacts
 * @warning MUST match order of `_os_guess` enum
 */
static char *os_strings[] = {
    N_("Windows 95"),
    N_("Windows NT 4.0"),
    N_("Windows 98"),
    N_("Windows ME"),
    N_("Windows 2000"),
    N_("Windows XP or 2003"),
    N_("Windows 2000, XP or 2003"),
    N_("Windows Vista - 8.1"),
    N_("Windows 10 or above")
};

static int          output_mode        = OUTPUT_NONE;
static gboolean     no_heading         = FALSE;
static gboolean     use_localtime      = FALSE;
       gboolean     live_mode          = FALSE;
static char        *delim              = NULL;
       char        *legacy_encoding    = NULL; /*!< INFO2 only, or upon request */
static char        *output_loc         = NULL;
static char        *tmppath            = NULL; /*!< used iff output_loc is defined */
       char       **fileargs           = NULL;
static FILE        *out_fh             = NULL; /*!< unused for Windows console */
static FILE        *err_fh             = NULL; /*!< unused for Windows console */

/*! These options are only effective for tab delimited mode output */
static const GOptionEntry text_options[] = {
    {
        "delimiter", 't', 0,
        G_OPTION_ARG_CALLBACK, _set_opt_delim,
        N_("String to use as delimiter (TAB by default)"), N_("STRING")
    },
    {
        "no-heading", 'n', G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, _set_opt_noheading,
        N_("Don't show column header and metadata"), NULL
    },
    {
        "always-utf8", '8', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, _option_deprecated,
        N_("(This option is deprecated)"), NULL
    },
    {NULL}
};

static const GOptionEntry main_options[] = {
    {
        "output", 'o', 0,
        G_OPTION_ARG_CALLBACK, _set_output_path,
        N_("Write output to FILE"), N_("FILE")
    },
    {
        "xml", 'x', G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, _set_output_xml,
        N_("Output in XML format instead of tab-delimited values"), NULL
    },
    {
        "localtime", 'z', 0,
        G_OPTION_ARG_NONE, &use_localtime,
        N_("Present deletion time in time zone of local system (default is UTC)"),
        NULL
    },
    {
        "version", 'v', G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, (GOptionArgFunc) print_version_and_exit,
        N_("Print version information and exit"), NULL
    },
    {
        G_OPTION_REMAINING, 0, 0,
        G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
        N_("INFO2 file name"), NULL
    },
    {NULL}
};

/*! Appended to main option group if program is INFO2 reader */
static const GOptionEntry rbinfile_options[] = {
    {
        "legacy-filename", 'l', 0,
        G_OPTION_ARG_CALLBACK, _check_legacy_encoding,
        N_("Show legacy (8.3) path if available and specify its CODEPAGE"),
        N_("CODEPAGE")
    },
    {NULL}
};

/* Append to main option group if program is $Recycle.bin reader */
static const GOptionEntry live_options[] = {
    {
        "live", 0, 0,
        G_OPTION_ARG_NONE, &live_mode,
        N_("Inspect live system"), NULL
    },
    {NULL}
};

/*
 * Option handling related routines
 */

static gboolean
_set_output_mode (int       mode,
                  GError  **err)
{
    if (output_mode == mode)
        return TRUE;

    if (output_mode == OUTPUT_NONE) {
        output_mode = mode;
        return TRUE;
    }

    g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
        _("Plain text format options can not be used in XML mode."));
    return FALSE;
}

static gboolean
_set_output_xml (const gchar *opt_name,
                 const gchar *value,
                 gpointer     data,
                 GError     **err)
{
    UNUSED(opt_name);
    UNUSED(value);
    UNUSED(data);

    return _set_output_mode (OUTPUT_XML, err);
}

static gboolean
_set_opt_noheading (const gchar *opt_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **err)
{
    UNUSED(opt_name);
    UNUSED(value);
    UNUSED(data);

    no_heading = TRUE;

    return _set_output_mode (OUTPUT_CSV, err);
}

/*!
 * single/double quotes and backslashes have already been
 * quoted / unquoted when parsing arguments. We need to
 * interpret \\r, \\n etc separately
 */
static char *
_filter_escapes (const char *str)
{
    GString *result, *debug_str;
    char *i = (char *) str;

    g_return_val_if_fail ( (str != NULL) && (*str != '\0'), NULL);

    result = g_string_new (NULL);
    do
    {
        if ( *i != '\\' )
        {
            result = g_string_append_c (result, *i);
            continue;
        }

        switch ( *(++i) )
        {
          case 'r':
            result = g_string_append_c (result, '\r'); break;
          case 'n':
            result = g_string_append_c (result, '\n'); break;
          case 't':
            result = g_string_append_c (result, '\t'); break;
          case 'e':
            result = g_string_append_c (result, '\x1B'); break;
          default:
            result = g_string_append_c (result, '\\'); i--;
        }
    }
    while ( *(++i) );

    debug_str = g_string_new ("filtered delimiter = ");
    i = result->str;
    do
    {
        if ( *i >= 0x20 && *i <= 0x7E )  /* problem during linking with g_ascii_isprint */
            debug_str = g_string_append_c (debug_str, *i);
        else
            g_string_append_printf (debug_str, "\\x%02X", *(unsigned char *) i);
    }
    while ( *(++i) );
    g_debug ("%s", debug_str->str);
    g_string_free (debug_str, TRUE);
    return g_string_free (result, FALSE);
}

static gboolean
_set_opt_delim (const gchar *opt_name,
               const gchar *value,
               gpointer     data,
               GError     **err)
{
    UNUSED(opt_name);
    UNUSED(data);

    static gboolean seen = FALSE;

    if (seen)
    {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple delimiter options disallowed."));
        return FALSE;
    }
    seen = TRUE;

    delim = (*value) ? _filter_escapes (value) : g_strdup ("");

    return _set_output_mode (OUTPUT_CSV, err);
}

static gboolean
_set_output_path (const gchar *opt_name,
                  const gchar *value,
                  gpointer     data,
                  GError     **err)
{
    UNUSED(opt_name);
    UNUSED(data);

    static gboolean seen     = FALSE;

    if (seen)
    {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple output destinations disallowed."));
        return FALSE;
    }
    seen = TRUE;

    if ( *value == '\0' )
    {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Empty output filename disallowed."));
        return FALSE;
    }

    if (g_file_test (value, G_FILE_TEST_EXISTS)) {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Output destinations already exists."));
        return FALSE;
    }

    output_loc = g_strdup (value);
    return TRUE;
}

static gboolean
_option_deprecated (const gchar *opt_name,
                    const gchar *unused,
                    gpointer     data,
                    GError     **err)
{
    UNUSED(unused);
    UNUSED(data);
    UNUSED(err);
    g_warning(_("Option '%s' is deprecated and ignored."), opt_name);
    return TRUE;
}

static gboolean
_check_legacy_encoding (const gchar *opt_name,
                        const gchar *enc,
                        gpointer     data,
                        GError     **err)
{
    UNUSED(opt_name);
    UNUSED(data);

    char           *s;
    gint            e;
    gboolean        ret      = FALSE;
    static gboolean seen     = FALSE;
    GError         *conv_err = NULL;

    if (seen)
    {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple encoding options disallowed."));
        return FALSE;
    }
    seen = TRUE;

    if ( *enc == '\0' )
    {
        g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Empty encoding option disallowed."));
        return FALSE;
    }

    s = g_convert ("C:\\", -1, "UTF-8", enc, NULL, NULL, &conv_err);

    if (conv_err == NULL)
    {
        if (strcmp ("C:\\", s) != 0) /* e.g. EBCDIC based code pages */
        {
            g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                _("'%s' can't possibly be a code page or compatible "
                "encoding used by localized Windows."), enc);
        } else {
            legacy_encoding = g_strdup (enc);
            ret = TRUE;
        }
        goto done_check_encoding;
    }

    e = conv_err->code;
    g_clear_error (&conv_err);

    switch (e)
    {
        case G_CONVERT_ERROR_NO_CONVERSION:

            g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                _("'%s' encoding is not supported by glib library "
                "on this system.  If iconv program is present on "
                "system, use 'iconv -l' for a list of possible "
                "alternatives; otherwise check out following site for "
                "a list of probable encodings to use:\n\n\t%s"), enc,
#ifdef G_OS_WIN32
                "https://github.com/win-iconv/win-iconv/blob/master/win_iconv.c"
#else
                "https://www.gnu.org/software/libiconv/"
#endif
            );
            break;

        /* Encodings not ASCII compatible can't possibly be ANSI/OEM code pages */
        case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
        case G_CONVERT_ERROR_PARTIAL_INPUT:

            g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                _("'%s' can't possibly be a code page or compatible "
                "encoding used by localized Windows."), enc);
            break;

        default:
            g_assert_not_reached ();
    }

done_check_encoding:

    g_free (s);
    return ret;
}

static gboolean
_count_fileargs (GOptionContext *context,
                 GOptionGroup   *group,
                 gpointer        data,
                 GError        **err)
{
    UNUSED (context);
    UNUSED (group);
    UNUSED (data);

    if (live_mode) {
        if (fileargs && g_strv_length (fileargs)) {
            g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                _("Live system probation must not be used together "
                  "with file arguments."));
            return FALSE;
        }
    }
    else
    {
        if (! fileargs || (g_strv_length (fileargs) != 1)) {
            g_set_error_literal (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                _("Must specify exactly one file or folder argument."));
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
_text_default_options (GOptionContext *context,
                       GOptionGroup   *group,
                       gpointer        data,
                       GError        **err)
{
    UNUSED (context);
    UNUSED (group);
    UNUSED (data);
    UNUSED (err);

    /* Fallback values after successful option parsing */
    if (delim == NULL)
        delim = g_strdup ("\t");

    if (output_mode == OUTPUT_NONE)
        output_mode = OUTPUT_CSV;

    return TRUE;
}

/*
 * Charset conversion routines
 */

size_t
ucs2_strnlen (const char *str, size_t max_sz)
{
#ifdef G_OS_WIN32

    return wcsnlen_s ((const wchar_t *) str, max_sz);

#else

    if (str == NULL)
        return 0;

    for (size_t i = 0; i < max_sz; i++) {
        if (*(str + i*2) == '\0' && *(str + i*2 + 1) == '\0')
            return i;
    }
    return max_sz;

#endif
}

static void
_advance_char (size_t       sz,
               gchar      **in_str,
               gsize       *read_bytes,
               gchar      **out_str,
               gsize       *write_bytes,
               const char  *tmpl)
{
    gchar *repl;

    switch (sz) {
        case 1:
        {
            unsigned char c = *(unsigned char *) (*in_str);
            repl = g_strdup_printf (tmpl, c);
        }
            break;

        case 2:
        {
            guint16 c = GUINT16_FROM_LE (*(guint16 *) (*in_str));
            repl = g_strdup_printf (tmpl, c);
        }
            break;

        default:
            g_assert_not_reached();
    }

    (*in_str) += sz;
    if (read_bytes != NULL)
        (*read_bytes) -= sz;

    *out_str = g_stpcpy (*out_str, (const char *) repl);
    if (write_bytes != NULL)
        *write_bytes -= strlen (repl);

    g_free (repl);
    return;
}

/*! Last argument is there to avoid recomputing */
static char *
_filter_printable_char (const char *str,
                        const char *tmpl)
{
    char     *p, *np;
    gunichar  c;
    GString  *s;

    s = g_string_sized_new (strlen (str) * 2);
    p = (char *) str;
    while (*p)
    {
        c  = g_utf8_get_char  (p);
        np = g_utf8_next_char (p);

        /*
         * ASCII space is the norm (e.g. Program Files), but
         * all other kinds of spaces are rare, so escape them too
         */
        if (g_unichar_isgraph (c) || (c == 0x20))
            s = g_string_append_len (s, p, (gssize) (np - p));
        else
            g_string_append_printf (s, tmpl, c);

        p = np;
    }

    return g_string_free (s, FALSE);
}

/*!
 * Converts a Windows path in specified legacy encoding or unicode
 * path into UTF-8 encoded version. When encoding error arises,
 * it attempts to be robust and substitute concerned bytes or
 * unicode codepoints with escaped ones specified by printf-style
 * template. This routine is not for generic charset conversion.
 *
 * 1. Caller is responsible to only supply non-stateful encoding
 * meant to be used as Windows code page, or use NULL to represent
 * UTF-16LE (the Windows unicode path encoding). Never supply
 * any unicode encoding directly.
 *
 * 2. Caller is responsible for using correct printf template
 * for desired data type, no check is done here.
 */
char *
conv_path_to_utf8_with_tmpl (const char *path,
                             const char *from_enc,
                             const char *tmpl,
                             size_t     *read,
                             r2status   *st)
{
    char *u8_path, *i_ptr, *o_ptr, *result = NULL;
    gsize len, r_total, rbyte, wbyte, status, in_ch_width, out_ch_width;
    GIConv conv;

    /* for UTF-16, first byte of str can be null */
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail ((from_enc == NULL) || (*from_enc != '\0'), NULL);
    g_return_val_if_fail ((    tmpl != NULL) && (    *tmpl != '\0'), NULL);

    /* try the template */
    {
        char *s = g_strdup_printf (tmpl, from_enc ? 0xFF : 0xFFFF);
        /* UTF-8 character occupies at most 6 bytes */
        out_ch_width = MAX (strlen(s), 6);
        g_free (s);
    }

    if (from_enc != NULL) {
        in_ch_width = sizeof (char);
        len = strnlen (path, WIN_PATH_MAX);
    } else {
        in_ch_width = sizeof (gunichar2);
        len = ucs2_strnlen (path, WIN_PATH_MAX);
    }

    if (! len)
        return NULL;

    rbyte   = len *  in_ch_width;
    wbyte   = len * out_ch_width;
    u8_path = g_malloc0 (wbyte);

    r_total = rbyte;
    i_ptr   = (char *) path;
    o_ptr   = u8_path;

    /* Shouldn't fail, from_enc already tested upon start of prog */
    conv = g_iconv_open ("UTF-8", from_enc ? from_enc : "UTF-16LE");

    g_debug ("Initial: read=%" G_GSIZE_FORMAT ", write=%" G_GSIZE_FORMAT,
            rbyte, wbyte);

    /* Pass 1: Convert whole string to UTF-8, all illegal seq become escaped hex */
    while (TRUE)
    {
        int e;

        if (*i_ptr == '\0') {
            if (from_enc   != NULL) break;
            if (*(i_ptr+1) == '\0') break; /* utf-16: check "\0\0" */
        }

        status = g_iconv (conv, &i_ptr, &rbyte, &o_ptr, &wbyte);
        e = errno;

        if ( status != (gsize) -1 ) break;

        g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
            ", stt=%" G_GSIZE_FORMAT " (%s) str=%s",
            rbyte, wbyte, status, strerror(e), u8_path);

        /* XXX Should I consider the possibility of odd bytes for EINVAL? */
        switch (e) {
            case EILSEQ:
            case EINVAL:
                _advance_char (in_ch_width, &i_ptr, &rbyte, &o_ptr, &wbyte, tmpl);
                /* reset state, hopefully Windows don't use stateful encoding at all */
                g_iconv (conv, NULL, NULL, &o_ptr, &wbyte);
                *st = R2_ERR_USER_ENCODING;
                break;
            case E2BIG:
                /* Should have already allocated enough buffer. Let it KABOOM! otherwise. */
                g_assert_not_reached();
        }
    }

    g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
        ", stt=%" G_GSIZE_FORMAT ", str=%s", rbyte, wbyte, status, u8_path);

    g_iconv_close (conv);

    if (read != NULL)
        *read = r_total - rbyte;

    /* Pass 2: Convert all ctrl characters (and some more) to hex */
    if (g_utf8_validate (u8_path, -1, NULL))
        result = _filter_printable_char (u8_path, tmpl);
    else {
        g_critical ("%s", _("Converted path failed UTF-8 validation"));
        *st = R2_ERR_INTERNAL;
    }

    g_free (u8_path);

    return result;
}

/*
 * Date / Time handling routines
 */

GDateTime *
win_filetime_to_gdatetime (int64_t win_filetime)
{
    int64_t t;

    /* Let's assume we don't need subsecond time resolution */
    t = (win_filetime - 116444736000000000LL) / 10000000;

    g_debug ("FileTime -> Epoch: %" G_GINT64_FORMAT
        " -> %" G_GINT64_FORMAT, win_filetime, t);

    return g_date_time_new_from_unix_utc (t);
}

// stdout / stderr handling, with special care
// on Windows command prompt.

static void
_local_print (gboolean    is_stdout,
              const char *str)
{
    FILE  *fh;

    if (!g_utf8_validate (str, -1, NULL)) {
        g_critical (_("String not in UTF-8 encoding: %s"), str);
        return;
    }

    fh = is_stdout ? out_fh : err_fh;

#ifdef G_OS_WIN32
    // See init_wincon_handle() for more info
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
_local_printout (const char *str)
{
    _local_print (TRUE, str);
}

static void
_local_printerr (const char *str)
{
    _local_print (FALSE, str);
}

void
rifiuti_init (void)
{
    setlocale (LC_ALL, "");

#ifdef G_OS_WIN32
    if (! init_wincon_handle (FALSE))
#endif
        err_fh = stderr;
    g_set_printerr_handler (_local_printerr);

#ifdef G_OS_WIN32
    if (! init_wincon_handle (TRUE))
#endif
        out_fh = stdout;
    g_set_print_handler (_local_printout);
}

void
rifiuti_setup_opt_ctx (GOptionContext **context,
                       rbin_type        type)
{
    char         *desc_str;
    GOptionGroup *group, *txt_group;

    desc_str = g_strdup_printf (
        _("Usage help: %s\nBug report: %s\nMore info : %s"),
        PROJECT_TOOL_USAGE_URL,
        PROJECT_BUG_REPORT_URL,
        PROJECT_GH_PAGE);
    g_option_context_set_description (*context, desc_str);
    g_free (desc_str);

    /* main group */
    group = g_option_group_new (NULL, NULL, NULL, NULL, NULL);

    g_option_group_add_entries (group, main_options);
    switch (type)
    {
        case RECYCLE_BIN_TYPE_FILE:
            g_option_group_add_entries (group, rbinfile_options);
            break;
        case RECYCLE_BIN_TYPE_DIR:
#ifdef G_OS_WIN32
            g_option_group_add_entries (group, live_options);
#else
            // TODO live inspection in WSL
            UNUSED (live_options);
#endif
            break;
        default: break;
    }

    g_option_group_set_parse_hooks (group, NULL, _count_fileargs);
    g_option_context_set_main_group (*context, group);

    /* text group */
    txt_group = g_option_group_new ("text",
        _("Plain text output options:"),
        N_("Show plain text output options"), NULL, NULL);

    g_option_group_add_entries (txt_group, text_options);
    g_option_group_set_parse_hooks (
        txt_group, NULL, _text_default_options);
    g_option_context_add_group (*context, txt_group);

    g_option_context_set_help_enabled (*context, TRUE);
}

/**
 * @brief Process command line arguments
 * @param context option context pointer
 * @param argv reference to command line `argv`
 * @param error reference of `GError` pointer to store errors
 * @return `R2_OK` upon parse success, `R2_ERR_ARG` on failure.
 *         `error` is set upon error as well.
 */
r2status
rifiuti_parse_opt_ctx (GOptionContext **context,
                       char          ***argv,
                       GError         **error)
{
    int       argc;
    char    **args;
    GArray   *arg_array;  // TODO GStrvBuilder since 2.68

    argc = g_strv_length (*argv);

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (*argv);
#endif

    arg_array = g_array_new (TRUE, TRUE, sizeof(gpointer));
    g_array_append_vals (arg_array, args, argc);
    if (argc == 1) {
        char *help_opt = g_strdup ("--help-all");
        g_array_append_val (arg_array, help_opt);
#ifdef G_OS_WIN32
        g_set_print_handler (gui_message);
#endif
    }

    {
        char *args_str = g_strjoinv("|", (char **) arg_array->data);
        g_debug("Calling args: %s", args_str);
        g_free(args_str);
    }

    g_option_context_parse_strv (*context,
        (char ***) &(arg_array->data), error);
    g_option_context_free (*context);
    g_array_unref (arg_array);

    return (*error != NULL) ? R2_ERR_ARG : R2_OK;
}


/*!
 * Wrapper of g_utf16_to_utf8 for big endian system.
 * Always assume string is nul-terminated. (Unused now?)
 */
char *
utf16le_to_utf8 (const gunichar2   *str,
                 glong             *items_read,
                 glong             *items_written,
                 GError           **error)
{
#if ((G_BYTE_ORDER) == (G_LITTLE_ENDIAN))
    return g_utf16_to_utf8 (str, -1, items_read, items_written, error);
#else

    gunichar2 *buf;
    char *ret;

    /* should be guaranteed to succeed */
    buf = (gunichar2 *) g_convert ((const char *) str, -1, "UTF-16BE",
                                   "UTF-16LE", NULL, NULL, NULL);
    ret = g_utf16_to_utf8 (buf, -1, items_read, items_written, error);
    g_free (buf);
    return ret;
#endif
}

/**
 * @brief Wrapper of `g_mkstemp()` that returns file handle
 * @param error A `GError` pointer to store potential problem
 * @return File handle of temp file, or `NULL` upon error
 */
static FILE *
_get_tempfile (GError **error)
{
    int     fd, e = 0;
    FILE   *fh;
    char   *tmpl;

    /* segfaults if string is pre-allocated in stack */
    tmpl = g_strdup ("rifiuti-XXXXXX");

    if (-1 == (fd = g_mkstemp(tmpl))) {
        e = errno;
        g_set_error_literal (error, G_FILE_ERROR,
            g_file_error_from_errno(e), g_strerror(e));
        return NULL;
    }

    if (NULL == (fh = fdopen (fd, "wb"))) {
        e = errno;
        g_set_error_literal (error, G_FILE_ERROR,
            g_file_error_from_errno(e), g_strerror(e));
        close (fd);
        return NULL;
    }

    tmppath = tmpl;
    return fh;
}

/*! Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
static gboolean
_populate_index_file_list (GSList     **list,
                           const char  *path,
                           GError     **error)
{
    GDir           *dir;
    const char     *direntry;
    char           *fname;
    GPatternSpec   *pattern1, *pattern2;

    /*
     * g_dir_open returns cryptic error message or even succeeds on Windows,
     * when in fact the directory content is inaccessible.
     */
#ifdef G_OS_WIN32
    if ( !can_list_win32_folder (path, error) ) {
        return FALSE;
    }
#endif

    if (NULL == (dir = g_dir_open (path, 0, error)))
        return FALSE;

    pattern1 = g_pattern_spec_new ("$I??????.*");
    pattern2 = g_pattern_spec_new ("$I??????");

    while ((direntry = g_dir_read_name (dir)) != NULL)
    {
#if GLIB_CHECK_VERSION (2, 70, 0)
        if (!g_pattern_spec_match_string (pattern1, direntry) &&
            !g_pattern_spec_match_string (pattern2, direntry))
            continue;
#else /* glib < 2.70 */
        if (!g_pattern_match_string (pattern1, direntry) &&
            !g_pattern_match_string (pattern2, direntry))
            continue;
#endif
        fname = g_build_filename (path, direntry, NULL);
        *list = g_slist_prepend (*list, fname);
    }

    g_dir_close (dir);

    g_pattern_spec_free (pattern1);
    g_pattern_spec_free (pattern2);

    return TRUE;
}


/**
 * @brief Search for desktop.ini in folder for hint of recycle bin
 * @param path The searched path
 * @return `TRUE` if `desktop.ini` found to contain recycle bin
 *         identifier, `FALSE` otherwise
 */
static gboolean
_found_desktop_ini (const char *path)
{
    char *filename = NULL, *content = NULL, *found = NULL;

    filename = g_build_filename (path, "desktop.ini", NULL);
    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    {
        g_free (filename);
        return FALSE;
    }

    if (g_file_get_contents (filename, &content, NULL, NULL))
        /* Don't bother parsing, we don't use the content at all */
        found = strstr (content, RECYCLE_BIN_CLSID);

    g_free (content);
    g_free (filename);
    return (found != NULL);
}


static _os_guess
_guess_windows_ver (const metarecord meta)
{
    if (meta.type == RECYCLE_BIN_TYPE_DIR) {
        /*
        * No attempt is made to distinguish difference for Vista - 8.1.
        * The corrupt filesize artifact on Vista can't be reproduced,
        * therefore must be very rare.
        */
        switch (meta.version)
        {
            case VERSION_VISTA: return OS_GUESS_VISTA;
            case VERSION_WIN10: return OS_GUESS_10;
            default:            return OS_GUESS_UNKNOWN;
        }
    }

    /*
     * INFO2 only below
     */

    switch (meta.version)
    {
        case VERSION_WIN95: return OS_GUESS_95;
        case VERSION_WIN98: return OS_GUESS_98;
        case VERSION_NT4  : return OS_GUESS_NT4;
        case VERSION_ME_03:
            /* TODO use symbolic name when 2 versions are merged */
            if (meta.recordsize == 280)
                return OS_GUESS_ME;

            if (meta.is_empty)
                return OS_GUESS_2K_03;

            return meta.fill_junk ? OS_GUESS_2K : OS_GUESS_XP_03;

        /* Not using OS_GUESS_UNKNOWN, INFO2 ceased to be used so
           detection logic won't change in future */
        default: g_assert_not_reached();
    }
}

/**
 * @brief Add potentially valid file(s) to list
 * @param path The file or folder to be checked
 * @param list A `GSList` pointer to store potential index files
 * to be validated later on
 * @param type Recycle bin type
 * @param isolated_index Pointer to `gboolean`, indicating whether
 * the concerned `path` is a single `$Recycle.bin` type index
 * taken out of its original folder. Can be `NULL`, which means
 * this check is not performed.
 * @param error A `GError` pointer to store potential problems
 * @return Exit status, which can potentially be used as global
 * exit status of program.
 */
int
check_file_args (const char  *path,
                 GSList     **list,
                 rbin_type    type,
                 gboolean    *isolated_index,
                 GError     **error)
{
    g_debug ("Start checking path '%s'...", path);

    g_return_val_if_fail ( (path != NULL) && (list != NULL), R2_ERR_INTERNAL );

    if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
            _("'%s' does not exist."), path);
        return R2_ERR_OPEN_FILE;
    }

    if ((type == RECYCLE_BIN_TYPE_DIR) &&
        g_file_test (path, G_FILE_TEST_IS_DIR))
    {
        if ( ! _populate_index_file_list (list, path, error) )
            return R2_ERR_OPEN_FILE;
        /*
         * last ditch effort: search for desktop.ini. Just print empty content
         * representing empty recycle bin if found.
         */
        if ( !*list && !_found_desktop_ini (path) )
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                _("No files with name pattern '%s' "
                "are found in directory."), "$Ixxxxxx.*");
            return R2_ERR_OPEN_FILE;
        }
    }
    else if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
        if (isolated_index && (type == RECYCLE_BIN_TYPE_DIR)) {
            char *parent_dir = g_path_get_dirname (path);
            *isolated_index = ! _found_desktop_ini (parent_dir);
            g_free (parent_dir);
        }
        *list = g_slist_prepend ( *list, g_strdup (path) );
    }
    else
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
            (type == RECYCLE_BIN_TYPE_DIR) ?
            _("'%s' is not a normal file or directory.") :
            _("'%s' is not a normal file."), path);
        return R2_ERR_OPEN_FILE;
    }
    return R2_OK;
}


/**
 * @brief  Prepare temp file handle if required
 * @param  error A `GError` pointer to store potential problem
 * @return Previous output file handle if temp file is
 *         created successfully, otherwise `NULL`
 * @note   This func is a noop if `output_loc` is not set via
 *         command line argument
 */
FILE *
prep_tempfile_if_needed (GError **error)
{
    FILE *new_fh = NULL, *prev_fh = NULL;

    if (!output_loc)
        return NULL;

    new_fh = _get_tempfile (error);
    if (*error)
        return NULL;

    prev_fh = out_fh;
    out_fh  = new_fh;
    return prev_fh;
}


/**
 * @brief Perform temp file cleanup actions if necessary
 * @param fh File handle to use, after temp file has been
 *        closed. Usually this should be the file handle
 *        returned by `prep_tempfile_if_needed()`. Can
 *        be `NULL`, which means no further output is
 *        necessary, or Windows console handle is used instead.
 * @param error A `GError` pointer to store potential problem
 */
void
clean_tempfile_if_needed (FILE *fh, GError **error)
{
    if (!tmppath)
        return;

    g_assert (output_loc != NULL);
    g_assert (out_fh != NULL);

    fclose (out_fh);
    out_fh = fh;

    if (0 == g_rename (tmppath, output_loc))
        return;

    int e = errno;

    g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
        _("%s.\nTemp file can't be moved to destination '%s', "),
        g_strerror(e), output_loc);
}


void
close_handles (void)
{
    if (out_fh != NULL)
        fclose (out_fh);
    if (err_fh != NULL)
        fclose (err_fh);

#ifdef G_OS_WIN32
    close_wincon_handle();
    close_winerr_handle();
#endif
}


static void
_print_csv_header (metarecord meta)
{
    char *rbin_path = g_filename_display_name (meta.filename);

    g_print (_("Recycle bin path: '%s'\n"), rbin_path);
    g_free (rbin_path);

    if (meta.version == VERSION_NOT_FOUND) {
        g_print ("%s\n", _("Version: ??? (empty folder)"));
    } else {
        g_print (_("Version: %" G_GUINT64_FORMAT "\n"), meta.version);
    }

    if (( meta.type == RECYCLE_BIN_TYPE_FILE ) && meta.total_entry)
    {
        g_print (_("Total entries ever existed: %d"), meta.total_entry);
        g_print ("\n");
    }

#ifdef G_OS_WIN32
    if (live_mode)
    {
        char *product_name = windows_product_name();

        if (product_name) {
            g_print (_("OS: %s"), product_name);
            g_free (product_name);
        } else {
            g_print ("%s", _("OS detection failed"));
        }
    }
    else
#endif
    {
        _os_guess g = _guess_windows_ver (meta);

        if (g == OS_GUESS_UNKNOWN)
            g_print ("%s", _("OS detection failed"));
        else
            g_print (_("OS Guess: %s"), gettext (os_strings[g]) );
    }

    g_print ("\n");

    // Deletion time for each entry may or may not be under DST.
    // Results have not been verified.
    {
        GDateTime *now;
        char      *tzname = NULL, *tznumeric = NULL;

        now = use_localtime ? g_date_time_new_now_local ():
                              g_date_time_new_now_utc   ();

#ifdef G_OS_WIN32
        if (use_localtime)
            tzname = get_win_timezone_name ();
#endif
        if (tzname == NULL)
            tzname = g_date_time_format (now, "%Z");

        tznumeric = g_date_time_format (now, "%z");

        g_print (_("Time zone: %s [%s]"), tzname, tznumeric);
        g_print ("\n");

        g_date_time_unref (now);
        g_free (tzname);
        g_free (tznumeric);
    }

    g_print ("\n");

    {
        GArray   *col_array;
        char     *headerline;
        char     *fields[] = {
            /* TRANSLATOR COMMENT: appears in column header */
            N_("Index"), N_("Deleted Time"), N_("Gone?"), N_("Size"), N_("Path"), NULL
        };

        col_array = g_array_sized_new (TRUE, TRUE, sizeof (gpointer), 5);
        for (char **col_ptr = fields; *col_ptr != NULL; col_ptr++) {
            // const char *t = gettext (*col_ptr++);
            g_array_append_val (col_array, *col_ptr);
        }

        headerline = g_strjoinv (delim, (char **) col_array->data);
        g_print ("%s\n", headerline);

        g_free (headerline);
        g_array_free (col_array, TRUE);
    }
}

static void
_print_xml_header (metarecord meta)
{
    char *rbin_path = g_filename_display_name (meta.filename);

    /* No proper way to report wrong version info yet */
    g_print (
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<recyclebin format=\"%s\" version=\"%" G_GINT64_FORMAT "\">\n"
        "  <filename><![CDATA[%s]]></filename>\n",
        ( meta.type == RECYCLE_BIN_TYPE_FILE ) ? "file" : "dir",
        MAX (meta.version, 0), rbin_path);
    g_free (rbin_path);
}


void
print_header (metarecord meta)
{
    if (no_heading) return;

    g_debug ("Entering %s()", __func__);

    switch (output_mode)
    {
        case OUTPUT_CSV:
            _print_csv_header (meta);
            break;

        case OUTPUT_XML:
            _print_xml_header (meta);
            break;

        default:
            g_assert_not_reached();
    }
    g_debug ("Leaving %s()", __func__);
}


void
print_record_cb (rbin_struct *record,
                 void        *data)
{
    UNUSED(data);

    char       *out_fname, *index, *size = NULL;
    char       *outstr = NULL, *deltime = NULL;
    GDateTime  *dt;

    g_return_if_fail (record != NULL);

    index = (record->meta->type == RECYCLE_BIN_TYPE_FILE) ?
        g_strdup_printf ("%u", record->index_n) :
        g_strdup (record->index_s);

    dt = use_localtime ? g_date_time_to_local (record->deltime):
                         g_date_time_ref      (record->deltime);

    if ( record->legacy_path != NULL )
        out_fname = g_strdup (record->legacy_path);
    else
    {
        out_fname = record->uni_path ?
            g_strdup (record->uni_path) :
            g_strdup (_("(File name not representable in UTF-8 encoding)"));
    }

    switch (output_mode)
    {
        case OUTPUT_CSV:

            deltime = g_date_time_format (dt, "%F %T");

            if ( record->filesize == G_MAXUINT64 ) /* faulty */
                size = g_strdup ("???");
            else
                size = g_strdup_printf ("%" G_GUINT64_FORMAT, record->filesize);

            const char *gone =
                record->gone == FILESTATUS_EXISTS ? "FALSE" :
                record->gone == FILESTATUS_GONE   ? "TRUE"  :
                                                    "???"   ;
            outstr = g_strjoin (delim, index, deltime, gone, size, out_fname, NULL);

            g_print ("%s\n", outstr);

            break;

        case OUTPUT_XML:
        {
            GString *s = g_string_new (NULL);

            deltime = use_localtime ? g_date_time_format (dt, "%FT%T%z" ):
                                      g_date_time_format (dt, "%FT%TZ");

            g_string_printf (s,
                "  <record index=\"%s\" time=\"%s\" gone=\"%s\"",
                index, deltime,
                (record->gone == FILESTATUS_GONE  ) ? "true" :
                (record->gone == FILESTATUS_EXISTS) ? "false":
                                                      "unknown");

            if ( record->filesize == G_MAXUINT64 ) /* faulty */
                g_string_append_printf (s, " size=\"-1\"");
            else
                g_string_append_printf (s,
                    " size=\"%" G_GUINT64_FORMAT "\"", record->filesize);

            g_string_append_printf (s, ">\n"
                "    <path><![CDATA[%s]]></path>\n"
                "  </record>\n", out_fname);

            outstr = g_string_free (s, FALSE);
            g_print ("%s", outstr);
        }
            break;

        default:
            g_assert_not_reached();
    }
    g_date_time_unref (dt);
    g_free (outstr);
    g_free (out_fname);
    g_free (deltime);
    g_free (size);
    g_free (index);
}


void
print_footer (void)
{
    switch (output_mode)
    {
        case OUTPUT_CSV:
            /* do nothing */
            break;

        case OUTPUT_XML:
            g_print ("%s", "</recyclebin>\n");
            break;

        default:
            g_assert_not_reached();
    }
}


void
print_version_and_exit (void)
{
    g_print ("%s %s\n", PROJECT_NAME, PROJECT_VERSION);
    g_print ("%s\n\n", PROJECT_DESCRIPTION);
    /* TRANSLATOR COMMENT: %s is software name */
    g_print (_("%s is released under Revised BSD License.\n"), PROJECT_NAME);
    /* TRANSLATOR COMMENT: 1st argument is software name, 2nd is official URL */
    g_print (_("More information can be found on\n\n\t%s\n"),
        PROJECT_HOMEPAGE_URL);

    exit (R2_OK);
}


void
free_record_cb (rbin_struct *record)
{
    if ( record->meta->type == RECYCLE_BIN_TYPE_DIR )
        g_free (record->index_s);
    g_date_time_unref (record->deltime);
    g_free (record->uni_path);
    g_free (record->legacy_path);
    g_free (record);
}


void
free_vars (void)
{
    g_strfreev (fileargs);
    g_free (output_loc);
    g_free (legacy_encoding);
    g_free (delim);
    g_free (tmppath);
}
