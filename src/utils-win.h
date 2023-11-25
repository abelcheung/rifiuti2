/*
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#ifndef _RIFIUTI_UTILS_WIN_H
#define _RIFIUTI_UTILS_WIN_H

#include <glib.h>

void       gui_message              (const char     *message );
char *     get_win_timezone_name    (void);
GSList *   enumerate_drive_bins     (void);
char *     windows_product_name     (void);
gboolean   can_list_win32_folder    (const char     *path,
                                     GError        **error);
gboolean   init_wincon_handle       (gboolean        is_stdout);
void       close_wincon_handle      (void);
void       close_winerr_handle      (void);
void       puts_wincon              (gboolean        is_stdout,
                                     const wchar_t  *wstr);

#endif
