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

#ifndef _UTILS_H
#define _UTILS_H

#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include <glib.h>

enum {
  RIFIUTI_ERR_ARG        = 1,
  RIFIUTI_ERR_OPEN_FILE     ,
  RIFIUTI_ERR_BROKEN_FILE   ,
  RIFIUTI_ERR_ENCODING
};

enum {
  OUTPUT_CSV,
  OUTPUT_XML
};

/* Glib doc is lying; GStatBuf not available until 2.25.
 * Use the definition as of 2.44 */
#if !GLIB_CHECK_VERSION(2,25,0)
#if (defined (__MINGW64_VERSION_MAJOR) || defined (_MSC_VER)) && !defined(_WIN64)
typedef struct _stat32 GStatBuf;
#else
typedef struct stat GStatBuf;
#endif
#endif

/* Most versions of recycle bin use full PATH_MAX (260 char) to represent file paths,
 * in either ANSI or Unicode variations, except Windows 10 which uses variable size.
 */
#define WIN_PATH_MAX 0x104

/* shared functions */
time_t win_filetime_to_epoch (uint64_t    win_filetime);

void   print_header          (FILE       *outfile     ,
                              char       *infilename  ,
                              uint32_t    version     ,
                              gboolean    is_info2    );

void   print_footer          (FILE       *outfile     );

void   maybe_convert_fprintf (FILE       *file        ,
                              const char *format      , ...);
#ifdef G_OS_WIN32
void   gui_message           (const char *message     );
#endif

#endif

/* vim: set sw=2 expandtab ts=2 : */
