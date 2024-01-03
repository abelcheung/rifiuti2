/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#pragma once

#include <glib.h>

void              init_handles               (void);
void              close_handles              (void);
bool              get_tempfile               (GError   **error);
bool              clean_tempfile             (char      *dest,
                                              GError   **error);
