/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#ifndef _RIFIUTI_UTILS_CONV_H
#define _RIFIUTI_UTILS_CONV_H

#include <stdbool.h>
#include <glib.h>

bool          enc_is_ascii_compatible     (const char       *enc,
                                           GError          **error);

size_t        ucs2_strnlen                (const char       *str,
                                           ssize_t           max_sz);

char *        conv_path_to_utf8_with_tmpl (const char       *path,
                                           ssize_t           pathlen,
                                           const char       *from_enc,
                                           const char       *tmpl,
                                           size_t           *read,
                                           GError          **error);

char *        filter_escapes              (const char       *str);

#endif
