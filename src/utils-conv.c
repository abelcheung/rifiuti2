/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "utils-error.h"
#include "utils-conv.h"


struct _fmt_data fmt[] = {
    // must match out_fmt enum order
    {
        .friendly_name = "unknown format",
        .fallback_tmpl = {"", "", ""},
    },
    {
        .friendly_name = "TSV format",
        .fallback_tmpl = {"<\\u%04X>", "<\\%02X>", "<\\u%04X>"},
    },
    {
        .friendly_name = "XML format",
        // All paths are placed inside CDATA, using entities
        // can be confusing
        .fallback_tmpl = {"<\\u%04X>", "<\\%02X>", "<\\u%04X>"},
    },
    {
        .friendly_name = "JSON format",
        .fallback_tmpl = {
            "",  // Unused, see json_escape()
            // JSON doesn't allow encoding raw byte data in strings
            // (must be proper characters)
            "<\\%02X>",
            // HACK \u sequence collides with path separator, which
            // will be processed in json escaping routine. Use a temp
            // char to avoid collision and convert it back later
            "*u%04X"
        },
    },
};


/**
 * @brief Try out if encoding is compatible to ASCII
 * @param enc The encoding to test
 * @param error Location to store error during trial
 * @return `true` if compatible, `false` otherwise
 * (including the case where encoding doesn't exist)
 */
bool
enc_is_ascii_compatible    (const char   *enc,
                            GError      **error)
{
    bool equal;
    char *s;

    g_return_val_if_fail (enc && *enc, false);

    s = g_convert ("C:\\", -1, "UTF-8", enc, NULL, NULL, error);
    equal = (0 == g_strcmp0 ("C:\\", (const char *)s));
    g_free (s);

    if (equal)
        return true;

    if (*error == NULL)
        // Encoding is ASCII incompatible (e.g. EBCDIC). Even if trial
        // convert doesn't fail, it would cause application error
        // later on. Treat that as conversion error for convenience.
        g_set_error_literal (error, G_CONVERT_ERROR,
            G_CONVERT_ERROR_ILLEGAL_SEQUENCE, "");
    return false;
}


/**
 * @brief Compute UCS2 string length like `wcslen()`
 * @param str The string to check (in `char*` !)
 * @param max_sz Maximum length to check, or use -1 to
 * denote the string is nul-terminated
 * @return Either number of UCS2 char for whole string,
 * or return `max_sz` when `max_sz` param is exceeded
 */
size_t
ucs2_strnlen   (const char   *str,
                ssize_t       max_sz)
{
    // wcsnlen_s should be equivalent except for boundary
    // cases we don't care about

    size_t i = 0;
    char *p = (char *) str;

    if (str == NULL)
        return 0;

    while (*p || *(p+1))
    {
        if (max_sz >= 0 && i >= (size_t) max_sz)
            break;
        i++;
        p += 2;
    }
    return i;
}


/**
 * @brief Move character pointer for specified bytes
 * @param sz Must be either 1 or 2, denoting broken byte or broken UCS2 character
 * @param in_str Reference to input string to be converted
 * @param read_bytes Reference to already read bytes count to keep track of
 * @param out_str Reference to output string to be appended
 * @param write_bytes Reference to writable bytes count to decrement
 * @param fmt_type Type of output format; see `fmt[]` for detail
 * @note This is the core of `conv_path_to_utf8_with_tmpl()` doing
 * error fallback, converting a single broken char to `printf` output.
 */
static void
_advance_octet    (size_t       sz,
                   char       **in_str,
                   size_t      *read_bytes,
                   char       **out_str,
                   size_t      *write_bytes,
                   out_fmt      fmt_type)
{
    char *repl;

    switch (sz) {
        case 1:
        {
            unsigned char c = *(unsigned char *) (*in_str);
            repl = g_strdup_printf (fmt[fmt_type].fallback_tmpl[sz], c);
        }
            break;

        case 2:
        {
            uint16_t c = GUINT16_FROM_LE (*(uint16_t *) (*in_str));
            repl = g_strdup_printf (fmt[fmt_type].fallback_tmpl[sz], c);
        }
            break;

        default:
            g_assert_not_reached();
    }

    *in_str += sz;
    *read_bytes -= sz;

    *out_str = g_stpcpy (*out_str, (const char *) repl);
    *write_bytes -= strlen (repl);

    g_free (repl);
    return;
}


/**
 * @brief Convert non-printable characters to escape sequences
 * @param str The original string to be converted
 * @param fmt_type Type of output format; see `fmt[]` for detail
 * @return Converted string, maybe containing escape sequences
 * @attention Caller is responsible for using correct template, no
 * error checking is performed. This template should handle a single
 * Windows unicode path character, which is in UTF-16LE encoding.
 */
static char *
_filter_printable_char   (const char   *str,
                          out_fmt       fmt_type)
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

        // ASCII space is common (e.g. "Program Files"), but not
        // for any other kinds of space or invisible char
        if (g_unichar_isgraph (c) || (c == 0x20))
            s = g_string_append_len (s, p, (size_t) (np - p));
        else
            g_string_append_printf (s, fmt[fmt_type].fallback_tmpl[0], c);

        p = np;
    }

    return g_string_free (s, FALSE);
}


static void
_expand_write_buf    (char     **buf,
                      char     **ptr,
                      size_t    *sz,
                      size_t    *bytes_left)
{
    size_t   used     = *ptr - *buf,
             added    = (*sz);

    *sz += added;
    *bytes_left += added;
    *buf = g_realloc (*buf, *sz);
    *ptr = *buf + used;

    // some OS has more secure memory allocator than others *sigh*
    memset (*ptr, 0, *sz - used);

    g_debug ("Realloc : w=%02zu/%02zu, str=%s",
        *sz - used, *sz, (char *) *buf);
}


/**
 * @brief Convert path to UTF-8 encoding with customizable fallback
 * @param path The path string to be converted
 * @param from_enc Either a legacy Windows ANSI encoding, or use
 * `NULL` to represent Windows wide char encoding (UTF-16LE)
 * @param fmt_type Type of output format; see `fmt[]` for detail
 * @param func String transform func for post processing; can be
 * `NULL`, which still does some internal filtering
 * @param error Location to store error upon problem
 * @return UTF-8 encoded path, or `NULL` if conversion error happens
 * @note This is very similar to `g_convert_with_fallback()`, but the
 * fallback is a `printf`-style string instead of a fixed string,
 * so that different fallback sequence can be used with various output
 * format.
 * @attention 1. This routine is not for generic charset conversion.
 * Extra transformation is intended for path display only.
 * @attention 1. Caller is responsible for using correct template,
 * no error checking is performed.
 */
char *
conv_path_to_utf8_with_tmpl (const char      *path,
                             const char      *from_enc,
                             out_fmt          fmt_type,
                             StrTransformFunc func,
                             GError         **error)
{
    char            *u8_path,
                    *i_ptr,
                    *o_ptr,
                    *result = NULL;
    size_t           i_size,
                     o_size,
                     i_left,
                     o_left,
                     char_sz;
    ssize_t          status;
    GIConv           conv;
    GPtrArray       *err_offsets;

    // For unicode path, the first char must be ASCII drive letter
    // or slash. And since it is in little endian, first byte is
    // always non-null
    g_return_val_if_fail (path       && *path    , NULL);
    g_return_val_if_fail (! from_enc || *from_enc, NULL);

    if (from_enc != NULL) {
        char_sz   = sizeof (char);
        i_size    = char_sz * (strnlen (path, WIN_PATH_MAX) + 1);
    } else {
        char_sz   = sizeof (gunichar2);
        i_size    = char_sz * (ucs2_strnlen (path, -1) + 1);
    }

    // Ballpark figure; likely need to realloc once or twice
    i_left    = o_left = o_size = i_size;
    u8_path   = g_malloc0 (o_size);
    i_ptr     = (char *) path;
    o_ptr     = u8_path;

    // Shouldn't fail, encoding already tested upon start of prog
    conv = g_iconv_open ("UTF-8", from_enc ? from_enc : "UTF-16LE");

    g_debug ("Initial : r=%02zu/%02zu, w=%02zu/%02zu",
        i_left, i_size, o_left, o_size);
    err_offsets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

    // Pass 1: Convert to UTF-8, all illegal seq become escaped hex

    while (true)
    {
        int e = 0;

        if (*i_ptr == '\0') {
            if (from_enc   != NULL) break;
            if (*(i_ptr+1) == '\0') break; /* utf-16: check "\0\0" */
        }

        // When non-reversible char are converted to \uFFFD, there
        // is nothing we can do. Just accept the status quo.
        status = g_iconv (conv, &i_ptr, &i_left, &o_ptr, &o_left);
        if (status != -1)
            break;

        e = errno;
        g_debug ("Progress: r=%02zu/%02zu, w=%02zu/%02zu, status=%zd (%s), str=%s",
            i_left, i_size, o_left, o_size, status, g_strerror(e), u8_path);

        switch (e)
        {
            case EINVAL:  // TODO Handle partial input for EINVAL
                g_assert_not_reached ();
            case EILSEQ:
            {
                size_t *processed = g_malloc (sizeof (size_t));
                *processed = i_size - i_left;
                g_ptr_array_add (err_offsets, processed);
            }
                // Only EILSEQ is reported when it happens with E2BIG,
                // need to handle latter manually
                if (o_left < MIN_WRITEBUF_SPACE)
                    _expand_write_buf (&u8_path, &o_ptr, &o_size, &o_left);
                _advance_octet (char_sz, &i_ptr, &i_left, &o_ptr, &o_left, fmt_type);
                g_iconv (conv, NULL, NULL, &o_ptr, &o_left);  // reset state
                break;
            case E2BIG:
                _expand_write_buf (&u8_path, &o_ptr, &o_size, &o_left);
                break;
        }
    }

    g_debug ("Finally : r=%02zu/%02zu, w=%02zu/%02zu, status=%zd, str=%s",
        i_left, i_size, o_left, o_size, status, u8_path);

    g_iconv_close (conv);

    if (error &&
        g_error_matches ((const GError *) (*error),
            R2_REC_ERROR, R2_REC_ERROR_CONV_PATH) &&
        err_offsets->len > 0)
    {
        // More detailed error message showing offsets
        char *old = (*error)->message;
        GString *s = g_string_new ((const char *) old);
        s = g_string_append (s, ", at offset:");
        for (size_t i = 0; i < err_offsets->len; i++)
        {
            g_string_append_printf (s, " %zu",
                *((size_t *) (err_offsets->pdata[i])));
        }
        (*error)->message = g_string_free (s, FALSE);
        g_free (old);
    }

    g_ptr_array_free (err_offsets, TRUE);

    // Pass 2: Post processing, e.g. convert non-printable chars to hex

    g_return_val_if_fail (g_utf8_validate (u8_path, -1, NULL), NULL);

    if (func == NULL)
        result = _filter_printable_char (u8_path, fmt_type);
    else
        result = func (u8_path);
    g_free (u8_path);

    return result;
}


/**
 * @brief Convert escape sequences in delimiters
 * @param str The original delimiter string
 * @return Escaped delimiter string
 * @note Similar to `g_strcompress()`, but only process a few
 * characters, unlike glib routine which converts all 8bit chars.
 * Currently handles `\\r`, `\\n`, `\\t` and `\\e`.
 */
char *
filter_escapes (const char *str)
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


char *
json_escape (const char *src)
{
    // TODO g_string_replace from glib 2.68 does it all

    char *p = (char *) src;
    GString *s = g_string_sized_new (strlen (src));

    while (*p) {
        gunichar c = g_utf8_get_char (p);
        switch (c)
        {
        // JSON does not need to escape asterisk. This is for
        // workaround in format template
        case '*' : s = g_string_append_c (s, '\\'); break;
        case '\\':
        // For all other chars below, they are actually disallowed
        // in Windows path. This is for the mischievous who
        // move data to other OS and rename
        case 0x22:
        case 0x27:
            s = g_string_append_c (s, '\\');
            s = g_string_append_c (s, c);
            break;
        case 0x08: s = g_string_append (s, "\\b"); break;
        case 0x09: s = g_string_append (s, "\\t"); break;
        case 0x0A: s = g_string_append (s, "\\n"); break;
        case 0x0B: s = g_string_append (s, "\\v"); break;
        case 0x0C: s = g_string_append (s, "\\f"); break;
        case 0x0D: s = g_string_append (s, "\\r"); break;
        default  :
            if (g_unichar_isgraph (c) || c == 0x20)
                s = g_string_append_unichar (s, c);
            else if (c < 0x10000)
                g_string_append_printf (s, "\\u%04X", c);
            else  // calculate surrogate
            {
                uint16_t high, low;
                high = 0xD800 + ((c - 0x10000) >> 10  );
                low  = 0xDC00 + ((c - 0x10000) & 0x3FF);
                g_string_append_printf (s, "\\u%04X\\u%04X", high, low);
            }
            break;
        }
        p = g_utf8_next_char (p);
    }
    return g_string_free (s, FALSE);
}

