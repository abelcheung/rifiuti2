/* vim: set sw=4 ts=4 noexpandtab : */
/*
 * Copyright (C) 2003, by Keith J. Jones.
 * Copyright (C) 2007-2019 Abel Cheung.
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

#ifndef _RIFIUTI_H
#define _RIFIUTI_H

#include "utils.h"

/* These offsets are relative to file start */
#define VERSION_OFFSET           0
#define KEPT_ENTRY_OFFSET        4
#define TOTAL_ENTRY_OFFSET       8
#define RECORD_SIZE_OFFSET      12
#define FILESIZE_SUM_OFFSET     16
#define RECORD_START_OFFSET     20

/* Following offsets are relative to start of each record */
#define LEGACY_FILENAME_OFFSET  0x0
#define RECORD_INDEX_OFFSET     WIN_PATH_MAX
#define DRIVE_LETTER_OFFSET     ((WIN_PATH_MAX) + 4)
#define FILETIME_OFFSET         ((WIN_PATH_MAX) + 8)
#define FILESIZE_OFFSET         ((WIN_PATH_MAX) + 16)
#define UNICODE_FILENAME_OFFSET ((WIN_PATH_MAX) + 20)

#define LEGACY_RECORD_SIZE      ((WIN_PATH_MAX) + 20)        /* 280 bytes */
#define UNICODE_RECORD_SIZE     ((WIN_PATH_MAX) * 3 + 20)    /* 800 bytes */

#endif
