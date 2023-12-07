/*
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include "config.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "utils.h"
#ifdef G_OS_WIN32
#  include "utils-win.h"
#endif
#ifdef __GLIBC__
#  include "utils-linux.h"
#endif

/* Our own error domain */

G_DEFINE_QUARK (rifiuti-misc-error-quark, rifiuti_fatal_error)
G_DEFINE_QUARK (rifiuti-record-error-quark, rifiuti_record_error)

/* Common function signature for option callbacks */
#define DECL_OPT_CALLBACK(func)          \
static gboolean func (       \
    const gchar   *opt_name, \
    const gchar   *value,    \
    gpointer       data,     \
    GError       **error)

DECL_OPT_CALLBACK(_check_legacy_encoding);
DECL_OPT_CALLBACK(_set_output_path);
DECL_OPT_CALLBACK(_option_deprecated);
DECL_OPT_CALLBACK(_set_opt_delim);
DECL_OPT_CALLBACK(_set_opt_noheading);
DECL_OPT_CALLBACK(_set_output_xml);
DECL_OPT_CALLBACK(_show_ver_and_exit);

/* pre-declared out of laziness */

static int
_check_file_args (const char  *path,
                  GSList     **list,
                  rbin_type    type,
                  gboolean    *isolated_index,
                  GError     **error);


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
static gboolean     live_mode          = FALSE;
static char        *delim              = NULL;
static char        *output_loc         = NULL;
static char       **fileargs           = NULL;
static FILE        *out_fh             = NULL; /*!< unused for Windows console */
static FILE        *err_fh             = NULL; /*!< unused for Windows console */

       GSList      *filelist           = NULL;
       gboolean     isolated_index     = FALSE;
       char        *legacy_encoding    = NULL; /*!< INFO2 only, or upon request */
       metarecord  *meta               = NULL;


/* Options intended for tab delimited mode output only */
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
    { 0 }
};

/* Global options for program */
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
        G_OPTION_ARG_CALLBACK, _show_ver_and_exit,
        N_("Print version information and exit"), NULL
    },
    {
        G_OPTION_REMAINING, 0, 0,
        G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
        N_("INFO2 file name"), NULL
    },
    { 0 }
};

/* Options only intended for INFO2 reader */
static const GOptionEntry rbinfile_options[] = {
    {
        "legacy-filename", 'l', 0,
        G_OPTION_ARG_CALLBACK, _check_legacy_encoding,
        N_("Show legacy (8.3) path if available and specify its CODEPAGE"),
        N_("CODEPAGE")
    },
    { 0 }
};

/* Options only intended for live system probation */
static const GOptionEntry live_options[] = {
    {
        "live", 0, 0,
        G_OPTION_ARG_NONE, &live_mode,
        N_("Inspect live system"), NULL
    },
    { 0 }
};

/* Following routines are command argument handling related */

static gboolean
_set_output_mode (int       mode,
                  GError  **error)
{
    if (output_mode == mode)
        return TRUE;

    if (output_mode == OUTPUT_NONE) {
        output_mode = mode;
        return TRUE;
    }

    g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
        _("Plain text format options can not be used in XML mode."));
    return FALSE;
}


/**
 * @brief Option callback for setting output mode to XML
 * @return `FALSE` if option conflict exists, `TRUE` otherwise
 */
static gboolean
_set_output_xml (const gchar *opt_name,
                 const gchar *value,
                 gpointer     data,
                 GError     **error)
{
    UNUSED(opt_name);
    UNUSED(value);
    UNUSED(data);

    return _set_output_mode (OUTPUT_XML, error);
}


/**
 * @brief Option callback for setting TSV header visibility
 * @return `FALSE` if option conflict exists, `TRUE` otherwise
 */
static gboolean
_set_opt_noheading (const gchar *opt_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **error)
{
    UNUSED(opt_name);
    UNUSED(value);
    UNUSED(data);

    no_heading = TRUE;

    return _set_output_mode (OUTPUT_CSV, error);
}


/**
 * @brief Extra level of escape for escape sequences in delimiters
 * @param str The original delimiter string
 * @return Escaped delimiter string
 * @note Delimiter needs another escape because it is later used
 * in `printf` routines. It handles `\\r`, `\\n`, `\\t` and `\\e`.
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


/**
 * @brief Option callback for setting field delimiter in TSV output
 * @return `FALSE` if duplicate options are found, `TRUE` otherwise
 */
static gboolean
_set_opt_delim (const gchar *opt_name,
               const gchar *value,
               gpointer     data,
               GError     **error)
{
    UNUSED(opt_name);
    UNUSED(data);

    static gboolean seen = FALSE;

    if (seen)
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple delimiter options disallowed."));
        return FALSE;
    }
    seen = TRUE;

    delim = (*value) ? _filter_escapes (value) : g_strdup ("");

    return _set_output_mode (OUTPUT_CSV, error);
}


/**
 * @brief Option callback to set output file location
 * @return `FALSE` if duplicate options are found, or
 * output file location already exists. `TRUE` otherwise.
 */
static gboolean
_set_output_path (const gchar *opt_name,
                  const gchar *value,
                  gpointer     data,
                  GError     **error)
{
    UNUSED(opt_name);
    UNUSED(data);

    static gboolean seen     = FALSE;

    if (seen)
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple output destinations disallowed."));
        return FALSE;
    }
    seen = TRUE;

    if ( *value == '\0' )
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Empty output filename disallowed."));
        return FALSE;
    }

    if (g_file_test (value, G_FILE_TEST_EXISTS)) {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Output destinations already exists."));
        return FALSE;
    }

    output_loc = g_strdup (value);
    return TRUE;
}


/**
 * @brief Emits warning when an argument is marked as deprecated
 * @return Always `TRUE`
 */
static gboolean
_option_deprecated (const gchar *opt_name,
                    const gchar *unused,
                    gpointer     data,
                    GError     **error)
{
    UNUSED(unused);
    UNUSED(data);
    UNUSED(error);
    g_warning(_("Option '%s' is deprecated and ignored."), opt_name);
    return TRUE;
}


/**
 * @brief Check if supplied legacy ANSI code page is valid
 * @return `TRUE` if supplied code page is usable, `FALSE` otherwise.
 * @note Code page is not validated against actual recycle bin record.
 */
static gboolean
_check_legacy_encoding (const gchar *opt_name,
                        const gchar *enc,
                        gpointer     data,
                        GError     **error)
{
    UNUSED(opt_name);
    UNUSED(data);

    static gboolean seen     = FALSE;
    GError         *conv_err = NULL;

    if (seen)
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Multiple encoding options disallowed."));
        return FALSE;
    }
    seen = TRUE;

    if ( *enc == '\0' )
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("Empty encoding option disallowed."));
        return FALSE;
    }

    {
        char *s = g_convert ("C:\\", -1, "UTF-8", enc, NULL, NULL, &conv_err);
        gboolean equal = ! g_strcmp0 ("C:\\", s);
        g_free (s);

        if (equal) {
            legacy_encoding = g_strdup (enc);
            return TRUE;
        }
    }

    /* everything below is error handling */

    if (conv_err == NULL) {
        // Encoding is ASCII incompatible (e.g. EBCDIC). Even if trial
        // convert doesn't fail, it would cause application error
        // later on. Treat that as conversion error for convenience.
        g_set_error_literal (&conv_err, G_CONVERT_ERROR,
            G_CONVERT_ERROR_ILLEGAL_SEQUENCE, "");
    }

    if (g_error_matches (conv_err, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION)) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
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
    } else if (
        g_error_matches (conv_err, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE) ||
        g_error_matches (conv_err, G_CONVERT_ERROR, G_CONVERT_ERROR_PARTIAL_INPUT)
    ) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            _("'%s' is incompatible to any Windows code page."), enc);
    } else
        g_assert_not_reached ();

    g_clear_error (&conv_err);
    return FALSE;
}


/**
 * @brief Print program version with some text, then exit
 */
static gboolean
_show_ver_and_exit (const gchar *opt_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **error)
{
    UNUSED (opt_name);
    UNUSED (value);
    UNUSED (data);
    UNUSED (error);

    g_print ("%s %s\n", PROJECT_NAME, PROJECT_VERSION);
    g_print ("%s\n\n", PROJECT_DESCRIPTION);
    /* TRANSLATOR COMMENT: %s is software name */
    g_print (_("%s is released under Revised BSD License.\n"), PROJECT_NAME);
    /* TRANSLATOR COMMENT: 1st argument is software name, 2nd is official URL */
    g_print (_("More information can be found on\n\n\t%s\n"),
        PROJECT_HOMEPAGE_URL);

    // OK I cheated, it is not returning at all.
    exit (R2_OK);
}


/**
 * @brief File argument check callback, after handling all arguments
 * @return `TRUE` if a unique file argument is used under common scenario,
 * or no file argument is provided in live mode. `FALSE` otherwise.
 */
static gboolean
_fileargs_handler (GOptionContext *context,
                   GOptionGroup   *group,
                   metarecord     *meta,
                   GError        **error)
{
    UNUSED (context);
    UNUSED (group);

    gsize fileargs_len = fileargs ? g_strv_length (fileargs) : 0;

    if (!live_mode)
    {
        if (fileargs_len != 1)
        {
            g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                _("Must specify exactly one file or folder argument."));
            return FALSE;
        }
        meta->filename = g_strdup (fileargs[0]);

        return _check_file_args (meta->filename, &filelist,
            meta->type, &isolated_index, error);
    }

    if (fileargs_len)
    {
        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            _("Live system probation must not be used together "
                "with file arguments."));
        return FALSE;
    }

#if (defined G_OS_WIN32 || defined __GLIBC__)
    {
        meta->filename = g_strdup ("(current system)");
        GSList *bindirs = enumerate_drive_bins();
        if (!bindirs)
        {
            g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                _("Live probation unsupported under this system; "
                "requires running under Windows or WSL distribution."));
            return FALSE;
        }

        GSList *ptr = bindirs;
        while (ptr) {
            // Ignore errors, pretty common that some folders don't
            // exist or are empty.
            _check_file_args (ptr->data, &filelist,
                meta->type, NULL, NULL);
            ptr = ptr->next;
        }
        g_slist_free_full (bindirs, g_free);
    }
#endif

    return TRUE;
}


/**
 * @brief post-callback after handling all text related arguments
 * @return Always `TRUE`, denoting success. It never fails.
 */
static gboolean
_text_default_options (GOptionContext *context,
                       GOptionGroup   *group,
                       gpointer        data,
                       GError        **error)
{
    UNUSED (context);
    UNUSED (group);
    UNUSED (data);
    UNUSED (error);

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


/**
 * @brief Move character pointer for specified bytes
 * @param sz Must be either 1 or 2, denoting broken byte or broken UCS2 character
 * @param in_str Reference to input string to be converted
 * @param read_bytes Reference to already read bytes count to keep track of
 * @param out_str Reference to output string to be appended
 * @param write_bytes Reference to writable bytes count to decrement
 * @param tmpl `printf` template to represent the broken character
 * @note This is the core of `conv_path_to_utf8_with_tmpl()` doing
 * error fallback, converting a single broken char to `printf` output.
 */
static void
_advance_octet (size_t       sz,
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


/**
 * @brief Convert non-printable characters to escape sequences
 * @param str The original string to be converted
 * @param tmpl `printf` template to represent non-printable chars
 * @return Converted string, maybe containing escape sequences
 * @attention Caller is responsible for using correct template, no
 * error checking is performed. This template should handle a single
 * Windows unicode path character, which is in UTF-16LE encoding.
 */
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


/**
 * @brief Convert path to UTF-8 encoding with customizable fallback
 * @param path The path string to be converted
 * @param from_enc Either a legacy Windows ANSI encoding, or use
 * `NULL` to represent Windows wide char encoding (UTF-16LE)
 * @param tmpl `printf`-style string template to represent broken character
 * @param read Reference to number of successfully read bytes
 * @param st Reference to exit status integer, modified if error happens
 * @return UTF-8 encoded path, or `NULL` if conversion error happens
 * @note This is very similar to `g_convert_with_fallback()`, but the
 * fallback is a `printf`-style string instead of a fixed string.
 * @attention 1. This routine is not for generic charset conversion.
 * Only supply encoding used in Windows ANSI code page, or use `NULL`
 * for unicode path.
 * @attention 1. Caller is responsible for using correct template, no
 * error checking is performed.
 * This template should handle either single- or double-octet, namely
 * `%u`, `%o`, `%d`, `%i`, `%x` and `%X`. `%c` is no good since byte
 * sequence concerned can't be converted to proper UTF-8 character.
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

    /* Pass 1: Convert to UTF-8, all illegal seq become escaped hex */
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
                _advance_octet (in_ch_width, &i_ptr, &rbyte, &o_ptr, &wbyte, tmpl);
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

    /* Pass 2: Convert all non-printable chars to hex */
    if (g_utf8_validate (u8_path, -1, NULL))
        result = _filter_printable_char (u8_path, tmpl);
    else {
        g_critical ("%s", _("Converted path failed UTF-8 validation"));
        *st = R2_ERR_INTERNAL;
    }

    g_free (u8_path);

    return result;
}


/**
 * @brief Converts Windows FILETIME number to glib counterpart
 * @param win_filetime The FILETIME integer to be converted
 * @return `GDateTime` with UTC timezone
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


/**
 * @brief Prepare for glib option group setup
 * @param context Pointer to option context to be modified
 * @param type Recycle bin type; some options may or may not be available depending on type
 * @param meta Pointer to metadata structure
 */
static void
_opt_ctxt_setup (GOptionContext **context,
                 rbin_type        type)
{
    char         *desc_str;
    GOptionGroup *group, *txt_group;

    /* FIXME Sneaky metadata modification! Think about cleaner way */
    meta->type = type;

    desc_str = g_strdup_printf (
        _("Usage help: %s\nBug report: %s\nMore info : %s"),
        PROJECT_TOOL_USAGE_URL,
        PROJECT_BUG_REPORT_URL,
        PROJECT_GH_PAGE);
    g_option_context_set_description (*context, desc_str);
    g_free (desc_str);

    /* main group */
    group = g_option_group_new (NULL, NULL, NULL, meta, NULL);

    g_option_group_add_entries (group, main_options);
    switch (type)
    {
        case RECYCLE_BIN_TYPE_FILE:
            g_option_group_add_entries (group, rbinfile_options);
            break;
        case RECYCLE_BIN_TYPE_DIR:
#if (defined G_OS_WIN32 || defined __GLIBC__)
            g_option_group_add_entries (group, live_options);
#else
            UNUSED (live_options);
#endif
            break;
        default: break;
    }

    g_option_group_set_parse_hooks (group, NULL,
        (GOptionParseFunc) _fileargs_handler);
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
 * @param context Reference of option context pointer
 * @param argv Reference of command line `argv`
 * @param error Reference of `GError` pointer to store errors
 * @return `TRUE` if argument parsing succeeds.
 * `FALSE` on failure, and sets `error` as well.
 */
static gboolean
_opt_ctxt_parse (GOptionContext **context,
                 char          ***argv,
                 GError         **error)
{
    gsize     argc;
    char    **argv_u8;

#ifdef G_OS_WIN32
    argv_u8 = g_win32_get_command_line ();
    UNUSED (argv);
#else
    argv_u8 = g_strdupv (*argv);
#endif

    argc = g_strv_length (argv_u8);
    if (argc == 1) {
        argv_u8 = g_realloc_n (argv_u8, argc + 2, sizeof(gpointer));
        argv_u8[argc++] = "--help-all";
        argv_u8[argc] = (void *) NULL;
#ifdef G_OS_WIN32
        g_set_print_handler (gui_message);
#endif
    }

    {
        char *args_str = g_strjoinv("|", argv_u8);
        g_debug("Calling argv_u8 (%zu): %s", argc, args_str);
        g_free(args_str);
    }

    g_option_context_parse_strv (*context, &argv_u8, error);
    g_option_context_free (*context);
    g_strfreev (argv_u8);

    return (*error == NULL);
}


/**
 * @brief Free all fields used in a single recycle bin record
 * @param record Pointer to the record structure
 */
static void
_free_record_cb (rbin_struct *record)
{
    g_free (record->index_s);
    g_date_time_unref (record->deltime);
    g_free (record->uni_path);
    g_free (record->legacy_path);
    g_clear_error (&record->error);
    g_free (record);
}


/**
 * @brief Initialize program setup
 */
gboolean
rifiuti_init (rbin_type  type,
              char      *usage_param,
              char      *usage_summary,
              char    ***argv,
              GError   **error)
{
    GOptionContext *context;

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

    /* Initialize metadata struct */
    meta = g_malloc0 (sizeof (metarecord));
    meta->records = g_ptr_array_new ();
    g_ptr_array_set_free_func (meta->records, (GDestroyNotify) _free_record_cb);
    meta->invalid_records = g_hash_table_new_full (
        g_str_hash,
        g_str_equal,
        (GDestroyNotify) g_free,
        (GDestroyNotify) g_clear_error
    );

    /* Parse command line arguments and generate help */
    context = g_option_context_new (usage_param);
    g_option_context_set_summary (context, usage_summary);
    _opt_ctxt_setup (&context, type);

    return _opt_ctxt_parse (&context, argv, error);
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
 * @param fh Reference to `FILE` pointer to store file handle
 * @param path Reference to char pointer to store temp file path
 * @param error A `GError` pointer to store potential problem
 * @return `TRUE` if successful, `FALSE` otherwise
 */
static gboolean
_get_tempfile (FILE   **fh,
               char   **path,
               GError **error)
{
    int     fd, e = 0;
    char   *tmpl;

    g_return_val_if_fail (fh   && ! *fh  , FALSE);
    g_return_val_if_fail (path && ! *path, FALSE);

    /* segfaults if string is pre-allocated in stack */
    tmpl = g_strdup ("rifiuti-XXXXXX");

    if (-1 == (fd = g_mkstemp(tmpl))) {
        e = errno;
        g_set_error_literal (error, G_FILE_ERROR,
            g_file_error_from_errno(e), g_strerror(e));
        return FALSE;
    }

    if (NULL == (*fh = fdopen (fd, "wb"))) {
        e = errno;
        g_set_error_literal (error, G_FILE_ERROR,
            g_file_error_from_errno(e), g_strerror(e));
        g_close (fd, NULL);
        return FALSE;
    }

    *path = tmpl;
    return TRUE;
}


/**
 * @brief Scan folder and add all index files for parsing
 * @param list Pointer to file list to be modified
 * @param path The folder to scan
 * @param error Pointer to `GError` for error reporting
 * @return `TRUE` on success, `FALSE` if folder can't be opened
 */
static gboolean
_populate_index_file_list (GSList     **list,
                           const char  *path,
                           GError     **error)
{
    GDir           *dir;
    const char     *direntry;
    char           *fname;
    GPatternSpec   *pattern1, *pattern2;

    // g_dir_open() returns cryptic error message or even succeeds on Windows,
    // when in fact the directory content is inaccessible.
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


/**
 * @brief Guess Windows version which generated recycle bin index file
 * @param meta Pointer to metadata structure
 * @return Enum constant representing approximate Windows version range
 */
static _os_guess
_guess_windows_ver (const metarecord *meta)
{
    if (meta->type == RECYCLE_BIN_TYPE_DIR) {
        /*
        * No attempt is made to distinguish difference for Vista - 8.1.
        * The corrupt filesize artifact on Vista can't be reproduced,
        * therefore must be very rare.
        */
        switch (meta->version)
        {
            case VERSION_VISTA: return OS_GUESS_VISTA;
            case VERSION_WIN10: return OS_GUESS_10;
            default:            return OS_GUESS_UNKNOWN;
        }
    }

    /* INFO2 only below */

    switch (meta->version)
    {
        case VERSION_WIN95: return OS_GUESS_95;
        case VERSION_WIN98: return OS_GUESS_98;
        case VERSION_NT4  : return OS_GUESS_NT4;
        case VERSION_ME_03:
            /* TODO use symbolic name when 2 versions are merged */
            if (meta->recordsize == 280)
                return OS_GUESS_ME;

            if (meta->records->len == 0)
                return OS_GUESS_2K_03;

            return meta->fill_junk ? OS_GUESS_2K : OS_GUESS_XP_03;

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
 * @return `TRUE` if input file/dir is valid, `FALSE` otherwise
 * @attention Successful result does not imply files are appended
 * to list, which is the case for empty recycle bin
 */
static gboolean
_check_file_args (const char  *path,
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
        return FALSE;
    }

    if ((type == RECYCLE_BIN_TYPE_DIR) &&
        g_file_test (path, G_FILE_TEST_IS_DIR))
    {
        if ( ! _populate_index_file_list (list, path, error) )
            return FALSE;
        /*
         * last ditch effort: search for desktop.ini. Just print empty content
         * representing empty recycle bin if found.
         */
        if ( !*list && !_found_desktop_ini (path) )
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                _("No files with name pattern '%s' "
                "are found in directory."), "$Ixxxxxx.*");
            return FALSE;
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
        return FALSE;
    }
    return TRUE;
}


/**
 * @brief Close all output / error file handles before exit
 */
static void
_close_handles (void)
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


/**
 * @brief Print preamble and column header for TSV output
 * @param meta Pointer to metadata structure
 */
static void
_print_csv_header (metarecord *meta)
{
    {
        char *rbin_path = g_filename_display_name (meta->filename);
        g_print (_("Recycle bin path: '%s'\n"), rbin_path);
        g_free (rbin_path);
    }

    if (meta->version == VERSION_NOT_FOUND) {
        g_print ("%s\n", _("Version: ??? (empty folder)"));
    } else {
        g_print (_("Version: %" G_GUINT64_FORMAT "\n"), meta->version);
    }

    if (( meta->type == RECYCLE_BIN_TYPE_FILE ) && meta->total_entry)
    {
        g_print (_("Total entries ever existed: %d"), meta->total_entry);
        g_print ("\n");
    }

#if (defined G_OS_WIN32 || defined __GLIBC__)
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
        char *fields[] = {
            /* TRANSLATOR COMMENT: appears in column header */
            N_("Index"), N_("Deleted Time"), N_("Gone?"), N_("Size"), N_("Path"), NULL
        };
        char *headerline = g_strjoinv (delim, fields);
        g_print ("%s\n", headerline);
        g_free (headerline);
    }
}


/**
 * @brief Print preamble for XML output
 * @param meta Pointer to metadata structure
 */
static void
_print_xml_header (metarecord *meta)
{
    GString *result;

    result = g_string_new ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    g_string_append_printf (result,
        "<recyclebin format=\"%s\"",
        ( meta->type == RECYCLE_BIN_TYPE_FILE ) ? "file" : "dir");

    if (meta->version >= 0)  /* can be found and not error */
        g_string_append_printf (result,
            " version=\"%" G_GINT64_FORMAT "\"",
            meta->version);

    if (meta->type == RECYCLE_BIN_TYPE_FILE && meta->total_entry > 0)
        g_string_append_printf (result,
            " ever_existed=\"%" G_GUINT32_FORMAT "\"",
            meta->total_entry);

    result = g_string_append (result, ">\n");

    {
        char *rbin_path = g_filename_display_name (meta->filename);
        g_string_append_printf (result,
            "  <filename><![CDATA[%s]]></filename>\n",
            rbin_path);
        g_free (rbin_path);
    }

    g_print ("%s", result->str);
    g_string_free (result, TRUE);
}


/**
 * @brief Stub routine for printing header
 * @note Calls other printing routine depending on output mode
 */
static void
_print_header (void)
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


/**
 * @brief Print content of each recycle bin record
 * @param record Pointer to each recycle bin record
 * @param meta Pointer to metadata structure
 */
static void
_print_record_cb (rbin_struct *record,
                  const metarecord *meta)
{
    char       *out_fname, *index, *size = NULL;
    char       *outstr = NULL, *deltime = NULL;
    GDateTime  *dt;

    g_return_if_fail (record != NULL);

    index = (meta->type == RECYCLE_BIN_TYPE_FILE) ?
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


/**
 * @brief Print footer of recycle bin data
 */
static void
_print_footer (void)
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


/**
 * @brief Dump all results to screen or designated output file
 * @param error Reference of `GError` pointer to store potential problem
 * @return `TRUE` if output writing is successful, `FALSE` otherwise
 */
gboolean
dump_content (GError **error)
{
    FILE *tmp_fh = NULL, *prev_fh = NULL;
    char *tmp_path = NULL;

    if (output_loc)
    {
        // TODO use g_file_set_contents_full in glib 2.66
        if (_get_tempfile (&tmp_fh, &tmp_path, error))
        {
            prev_fh = out_fh;
            out_fh  = tmp_fh;
        }
        else
            return FALSE;
    }

    _print_header ();
    g_ptr_array_foreach (meta->records, (GFunc) _print_record_cb, meta);
    _print_footer ();

    if (!tmp_path)
        return TRUE;

    if (prev_fh)
    {
        fclose (out_fh);
        out_fh = prev_fh;
    }

    if (0 == g_rename (tmp_path, output_loc))
    {
        g_free (tmp_path);
        return TRUE;
    }

    g_free (tmp_path);
    int e = errno;
    g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
        _("%s.\nTemp file can't be moved to destination '%s', "),
        g_strerror(e), output_loc);
    return FALSE;
}


/**
 * @brief Final cleanup before exiting program, e.g. free all variables
 * @param meta Pointer to metadata structure
 */
void
rifiuti_cleanup (void)
{
    g_debug ("Cleaning up...");

    g_ptr_array_unref (meta->records);
    g_hash_table_destroy (meta->invalid_records);
    g_free (meta->filename);
    g_free (meta);

    g_slist_free_full (filelist, (GDestroyNotify) g_free);
    g_strfreev (fileargs);
    g_free (output_loc);
    g_free (legacy_encoding);
    g_free (delim);

    _close_handles ();
}
