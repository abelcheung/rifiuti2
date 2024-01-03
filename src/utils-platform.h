/*
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#pragma once

#include <inttypes.h>
#include <glib.h>

#ifdef G_OS_WIN32
void       gui_message              (const char     *message);
char *     get_win_timezone_name    (void);
gboolean   can_list_win32_folder    (const char     *path,
                                     GError        **error);
gboolean   init_wincon_handle       (gboolean        is_stdout);
void       puts_wincon              (gboolean        is_stdout,
                                     const wchar_t  *wstr);
void       cleanup_windows_res      (void);
#endif

#if (defined G_OS_WIN32 || defined __GLIBC__)
GPtrArray *enumerate_drive_bins     (GError        **error);
char *     windows_product_name     (void);
#endif

