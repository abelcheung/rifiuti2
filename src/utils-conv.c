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

#include "utils-conv.h"


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
 * @param tmpl `printf` template to represent the broken character
 * @note This is the core of `conv_path_to_utf8_with_tmpl()` doing
 * error fallback, converting a single broken char to `printf` output.
 */
static void
_advance_octet    (size_t       sz,
                   char       **in_str,
                   gsize       *read_bytes,
                   char       **out_str,
                   gsize       *write_bytes,
                   const char  *tmpl)
{
    char *repl;

    switch (sz) {
        case 1:
        {
            unsigned char c = *(unsigned char *) (*in_str);
            repl = g_strdup_printf (tmpl, c);
        }
            break;

        case 2:
        {
            uint16_t c = GUINT16_FROM_LE (*(uint16_t *) (*in_str));
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
 * @param tmpl `printf`-style string template to represent broken
 * character. This template should handle either single- or
 * double-octet, namely `%u`, `%o`, `%d`, `%i`, `%x` and `%X`.
 * @param read Reference to number of successfully read bytes
 * @param error Location to store error upon problem
 * @return UTF-8 encoded path, or `NULL` if conversion error happens
 * @note This is very similar to `g_convert_with_fallback()`, but the
 * fallback is a `printf`-style string instead of a fixed string,
 * so that different fallback sequence can be used with various output
 * format.
 * @attention 1. This routine is not for generic charset conversion.
 * Extra transformation is intended for path display only.
 * @attention 1. Caller is responsible for using correct template,
 * almost no error checking is performed.
 */
char *
conv_path_to_utf8_with_tmpl (const char *path,
                             ssize_t     pathlen,
                             const char *from_enc,
                             const char *tmpl,
                             size_t     *read,
                             GError    **error)
{
    char *u8_path, *i_ptr, *o_ptr, *result = NULL;
    gsize len, r_total, rbyte, wbyte, status, in_ch_width, out_ch_width;
    GIConv conv;

    g_return_val_if_fail (path && *path, NULL);
    g_return_val_if_fail (tmpl && *tmpl, NULL);
    g_return_val_if_fail (! from_enc || *from_enc, NULL);
    g_return_val_if_fail (! error    || ! *error , NULL);

    /* try the template */
    {
        char *s = g_strdup_printf (tmpl, from_enc ? 0xFF : 0xFFFF);
        /* UTF-8 character occupies at most 6 bytes */
        out_ch_width = MAX (strlen(s), 6);
        g_free (s);
    }

    if (from_enc != NULL) {
        in_ch_width = sizeof (char);
        len = strnlen (path, (size_t) pathlen);
    } else {
        in_ch_width = sizeof (gunichar2);
        len = ucs2_strnlen (path, (size_t) pathlen);
    }

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

        // GNU iconv may return number of nonreversible conversions
        // upon success, but we don't need to worry about it, as
        // conversion from code page to UTF-8 would not be nonreversible
        if ((gsize) -1 != (status = g_iconv (
            conv, &i_ptr, &rbyte, &o_ptr, &wbyte)))
            break;

        e = errno;

        g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
            ", stt=%" G_GSIZE_FORMAT " (%s) str=%s",
            rbyte, wbyte, status, g_strerror(e), u8_path);

        switch (e) {
            case EILSEQ:
            case EINVAL:  // TODO Handle partial input for EINVAL
                if (error && ! *error) {
                    g_set_error (error, G_CONVERT_ERROR,
                        G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                        _("Illegal sequence or partial input at offset %" G_GSIZE_FORMAT), rbyte);
                }
                _advance_octet (in_ch_width, &i_ptr, &rbyte, &o_ptr, &wbyte, tmpl);
                g_iconv (conv, NULL, NULL, &o_ptr, &wbyte);  // reset state
                break;
            case E2BIG:  // TODO realloc instead of Kaboom!
                g_assert_not_reached();
        }
    }

    g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
        ", stt=%" G_GSIZE_FORMAT ", str=%s", rbyte, wbyte, status, u8_path);

    g_iconv_close (conv);

    if (read != NULL)
        *read = r_total - rbyte;

    /* Pass 2: Convert all non-printable chars to hex */
    g_return_val_if_fail (g_utf8_validate (u8_path, -1, NULL), NULL);

    result = _filter_printable_char (u8_path, tmpl);
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
json_escape_path (const char *path)
{
    // TODO g_string_replace from glib 2.68 does it all

    char *p = (char *) path;
    gunichar c = 0;
    GString *s = g_string_new ("");

    while (*p) {
        c = g_utf8_get_char (p);
        if (c == '\\')
            s = g_string_append (s, "\\\\");
        else if (c == '*')
            s = g_string_append_c (s, '\\');
        else
            s = g_string_append_unichar (s, c);
        p = g_utf8_next_char (p);
    }
    return g_string_free (s, FALSE);
}

