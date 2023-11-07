/* vim: set sw=4 ts=4 noexpandtab : */
/*
 * Copyright (C) 2007-2023, Abel Cheung.
 * This package is released under Revised BSD License.
 * Please see docs/LICENSE.txt for more info.
 */

#ifndef _RIFIUTI_UTILS_WIN_H
#define _RIFIUTI_UTILS_WIN_H

#include <glib.h>

void       gui_message              (const char     *message );
char *     get_win_timezone_name    (void);
gboolean   can_list_win32_folder    (const char     *dir     );
gboolean   init_wincon_handle       (gboolean        is_stdout);
void       close_wincon_handle      (void);
void       close_winerr_handle      (void);
void       puts_wincon              (gboolean        is_stdout,
                                     const wchar_t  *wstr);

#endif
