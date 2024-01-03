/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "utils-error.h"
#include "utils-conv.h"


struct _fmt_data fmt[] = {
    // must match out_fmt enum order
    {
        .friendly_name = "TSV format",
        .fallback_tmpl = {"<\\u%04X>", "<\\%02X>", "<\\u%04X>"},
        .gone_outtext  = {"???", "FALSE", "TRUE"},
    },
    {
        .friendly_name = "XML format",
        // All paths are placed inside CDATA, using entities
        // can be confusing
        .fallback_tmpl = {"<\\u%04X>", "<\\%02X>", "<\\u%04X>"},
        .gone_outtext  = {"unknown", "false", "true"},
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
        .gone_outtext  = {"null", "false", "true"},
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
 * @brief Find null terminator position in UCS2 string
 * @param str The string to check (in `char *` !)
 * @param max_sz Maximum byte length to check, or use -1 to
 * denote the string should be nul-terminated
 * @return Byte position where null terminator (double \\0)
 * is found, or `max_sz` otherwise
 * @note Being different from standard C funcs like `wcsnlen()`
 * or `strnlen()`, it returns bytes, not chars. And it would
 * take care of odd bytes when UCS2 strings are expecting
 * even number of bytes.
 */
size_t
ucs2_bytelen   (const char   *str,
                ssize_t       max_sz)
{
    char *p = (char *) str;

    if (str == NULL || max_sz == 0)
        return 0;

    if (max_sz == 1)
        return 1;

    while (*p || *(p+1))
    {
        p += 2;
        if (max_sz >= 0 && p - str + 1 >= max_sz)
            return max_sz;
    }
    return p - str;
}


/**
 * @brief Move character pointer for specified bytes
 * @param sz Must be either 1 or 2, denoting broken byte or broken UCS2 character
 * @param ptr Location of char pointer to string to be converted
 * @param bytes_left Location to number of remaining bytes to read
 * @param s Broken byte(s) will be formatted and appended to this `GString`
 * @param fmt_type Type of output format; see `fmt[]` for detail
 * @note This is the core of `conv_path_to_utf8_with_tmpl()` doing
 * error fallback, converting a single broken char to `printf` output.
 */
static void
_advance_octet    (size_t       sz,
                   char       **ptr,
                   gsize       *bytes_left,
                   GString     *s,
                   out_fmt      fmt_type)
{
    int c = 0;

    g_return_if_fail (*bytes_left > 0);
    g_return_if_fail (sz == 1 || sz == 2);
    g_return_if_fail (*ptr != NULL);

    if (*bytes_left == 1)
        sz = 1;

    if (sz == 1)
        c = *(uint8_t *) (*ptr);
    else
        c = GUINT16_FROM_LE (*(uint16_t *) (*ptr));

    g_string_append_printf (s,
        fmt[fmt_type].fallback_tmpl[sz], c);

    *ptr += sz;
    *bytes_left -= sz;
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
_sync_pos   (GString   *str,
             gsize     *bytes_left,
             char     **chr_ptr,
             bool       from_gstring)
{
    if (from_gstring)
    {
        *bytes_left = str->allocated_len - str->len - 1;
        *chr_ptr = str->str + str->len;
    }
    else
    {
        str->len = str->allocated_len - *bytes_left - 1;
        g_assert (*chr_ptr == str->str + str->len);
        str->str[str->len] = '\0';
    }
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
conv_path_to_utf8_with_tmpl (const GString   *path,
                             const char      *from_enc,
                             out_fmt          fmt_type,
                             StrTransformFunc func,
                             GError         **error)
{
    char            *i_ptr,
                    *o_ptr,
                    *result;
    gsize            i_size,
                     i_left,
                     o_left,
                     char_sz,
                     status;
    GIConv           conv;
    GPtrArray       *err_offsets;
    GString         *s;

    // For unicode path, the first char must be ASCII drive letter
    // or slash. And since it is in little endian, first byte is
    // always non-null
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (! from_enc || *from_enc, NULL);

    if (from_enc)
    {
        char_sz = sizeof (char);
        i_left = i_size = strnlen (path->str, WIN_PATH_MAX);
    }
    else
    {
        char_sz = sizeof (gunichar2);
        i_left = i_size = ucs2_bytelen (path->str, path->len);
    }
    i_ptr = path->str;

    // Ballpark figure, GString decides alloc size on its own
    s = g_string_sized_new (i_size + 1);
    _sync_pos (s, &o_left, &o_ptr, true);

    // Shouldn't fail, encoding already tested upon start of prog
    conv = g_iconv_open ("UTF-8", from_enc ? from_enc : "UTF-16LE");

    g_debug ("Initial : r=%02zu, w=%02zu/%02zu",
        i_left, o_left, s->allocated_len - 1);
    err_offsets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

    // Pass 1: Convert to UTF-8, all illegal seq become escaped hex

    while (i_left > 0)
    {
        if (*i_ptr == '\0') {
            if (from_enc   != NULL) break;
            if (*(i_ptr+1) == '\0') break; /* utf-16: check "\0\0" */
        }

        // When non-reversible char are converted to \uFFFD, there
        // is nothing we can do. Just accept the status quo.
        status = g_iconv (conv, &i_ptr, &i_left, &o_ptr, &o_left);
        _sync_pos (s, &o_left, &o_ptr, false);
        if (status != (gsize) -1)
            break;

        int e = errno;
        g_debug ("Progress: r=%02zu, w=%02zu/%02zu, status=%zd (%s), str=%s",
            i_left, o_left, s->allocated_len - 1,
            status, g_strerror(e), s->str);

        switch (e)
        {
        case EINVAL:
        case EILSEQ:
        {
            size_t *processed = g_malloc (sizeof (size_t));
            *processed = i_size - i_left;
            g_ptr_array_add (err_offsets, processed);
        }
            _advance_octet (char_sz, &i_ptr, &i_left, s, fmt_type);
            _sync_pos (s, &o_left, &o_ptr, true);
            g_debug ("Progress: r=%02zu, w=%02zu/%02zu, str=%s",
                i_left, o_left, s->allocated_len - 1, s->str);
            g_iconv (conv, NULL, NULL, &o_ptr, &o_left);  // reset state
            _sync_pos (s, &o_left, &o_ptr, false);
            break;
        case E2BIG:
            s = g_string_set_size (s, s->allocated_len * 2);
            _sync_pos (s, &o_left, &o_ptr, true);
            break;
        }
    }

    g_debug ("Finally : r=%02zu, w=%02zu/%02zu, status=%zd, str=%s",
        i_left, o_left, s->allocated_len - 1, status, s->str);

    g_iconv_close (conv);

    if (error &&
        g_error_matches ((const GError *) (*error),
            R2_REC_ERROR, R2_REC_ERROR_CONV_PATH) &&
        err_offsets->len > 0)
    {
        // More detailed error message showing offsets
        char *old = (*error)->message;
        GString *dbg_str = g_string_new ((const char *) old);
        dbg_str = g_string_append (dbg_str, ", at offset:");
        for (size_t i = 0; i < err_offsets->len; i++)
        {
            g_string_append_printf (dbg_str, " %zu",
                *((size_t *) (err_offsets->pdata[i])));
        }
        (*error)->message = g_string_free (dbg_str, FALSE);
        g_free (old);
    }

    g_ptr_array_free (err_offsets, TRUE);

    // Pass 2: Post processing, e.g. convert non-printable chars to hex

    g_return_val_if_fail (g_utf8_validate (s->str, -1, NULL), NULL);

    if (func == NULL)
        result = _filter_printable_char (s->str, fmt_type);
    else
        result = func (s->str);
    g_string_free (s, TRUE);

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

