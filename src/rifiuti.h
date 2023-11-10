/*
 * Copyright (C) 2003, Keith J. Jones.
 * Copyright (C) 2007-2023, Abel Cheung.
 * This package is released under Revised BSD License.
 * Please see docs/LICENSE.txt for more info.
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
