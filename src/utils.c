/*
 * Copyright (C) 2007, 2015 Abel Cheung.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "utils.h"

static char *convert_with_fallback (const char *string, const char *fallback)
{
  GError *err = NULL;
  char *output = g_locale_from_utf8 (string, -1, NULL, NULL, &err);
  if (err != NULL)
  {
    g_critical ("Failed to convert message to display: %s\n", err->message);
    g_error_free (err);
    return g_strdup (fallback);
  }

  return output;
}

time_t win_filetime_to_epoch (uint64_t win_filetime)
{
  uint64_t epoch;

  /* I suppose millisecond resolution is not needed? -- Abel */
  epoch = (win_filetime - 116444736000000000LL) / 10000000;

  /* Will it go wrong? Hope not. */
  return (time_t) (epoch & 0xFFFFFFFF);
}

/* GUI message box */
#ifdef G_OS_WIN32

#include <windows.h>
#include <glib/gi18n.h>

void gui_message (const char *message)
{
  char *title, *output;

  title = convert_with_fallback (_("This is a command line application"),
      "This is a command line application");
  output = convert_with_fallback (message,
      "Fail to display help message. Please invoke program with '--help' option.");

  MessageBox (NULL, output, title, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
  g_free (title);
  g_free (output);
}

#endif

/* vim: set sw=2 expandtab ts=2 : */
