/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#ifndef _RIFIUTI_UTILS_CONV_H
#define _RIFIUTI_UTILS_CONV_H

#include <stdbool.h>
#include <glib.h>

// All versions of recycle bin prior to Windows 10 use full PATH_MAX
// or FILENAME_MAX (260 char) to store file paths in either ANSI or
// Unicode variations. However it is impossible to reuse any similar
// constant as it is totally platform dependent.
#define WIN_PATH_MAX 260

// Minimum bytes needed for a single utf8 character
#define MIN_WRITEBUF_SPACE 4

bool          enc_is_ascii_compatible     (const char       *enc,
                                           GError          **error);

size_t        ucs2_strnlen                (const char       *str,
                                           ssize_t           max_sz);

char *        conv_path_to_utf8_with_tmpl (const char       *path,
                                           const char       *from_enc,
                                           const char       *tmpl,
                                           size_t           *read,
                                           GError          **error);

char *        filter_escapes              (const char       *str);

char *        json_escape_path            (const char       *path);

#endif
