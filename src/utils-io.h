/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#ifndef _RIFIUTI_UTILS_IO_H
#define _RIFIUTI_UTILS_IO_H

#include <glib.h>

void              init_handles               (void);
void              close_handles              (void);
bool              get_tempfile               (GError   **error);
bool              clean_tempfile             (char      *dest,
                                              GError   **error);

#endif
