/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#pragma once

#include <stdbool.h>
#include <glib.h>

// All versions of recycle bin prior to Windows 10 use full PATH_MAX
// or FILENAME_MAX (260 char) to store file paths in either ANSI or
// Unicode variations. However it is impossible to reuse any similar
// constant as it is totally platform dependent.
#define WIN_PATH_MAX 260


// Minimum bytes needed to guarantee writing a utf8 character
#define MIN_WRITEBUF_SPACE 4


typedef enum
{
    FORMAT_UNKNOWN = -1,
    FORMAT_TEXT,
    FORMAT_XML,
    FORMAT_JSON,
} out_fmt;


typedef struct _fmt_data {
    const char *friendly_name;

    // tmpl[0]=utf8 (max 32bit), 1=char (8bit), 2=ucs2 (16bit)
    // templates should use numeric printf format since
    // they are not proper characters, or non-printable
    // chars in case of UTF-8
    // namely `%u`, `%o`, `%d`, `%i`, `%x` and `%X`
    const char *fallback_tmpl[3];

    // The output for file deletion status
    const char *gone_outtext[3];
} _fmt_data;


typedef
char *      (*StrTransformFunc)           (const char       *src);


bool          enc_is_ascii_compatible     (const char       *enc,
                                           GError          **error);

size_t        ucs2_bytelen                (const char       *str,
                                           ssize_t           max_sz);

char *        conv_path_to_utf8_with_tmpl (const GString    *path,
                                           const char       *from_enc,
                                           out_fmt           fmt_type,
                                           StrTransformFunc  func,
                                           GError          **error);

char *        filter_escapes              (const char       *str);

char *        json_escape                 (const char       *src);

