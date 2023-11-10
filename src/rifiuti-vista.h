/*
 * Copyright (C) 2007-2023, Abel Cheung
 * This package is released under Revised BSD License.
 * Please see docs/LICENSE.txt for more info.
 */

#ifndef _RIFIUTI_VISTA_H
#define _RIFIUTI_VISTA_H

#include "utils.h"

#define VERSION_OFFSET               0x0
#define FILESIZE_OFFSET              0x8
#define FILETIME_OFFSET              0x10
#define VERSION1_FILENAME_OFFSET     0x18
#define VERSION2_FILENAME_OFFSET     0x1C

#define VERSION1_FILE_SIZE           ((VERSION1_FILENAME_OFFSET) + (WIN_PATH_MAX) * 2)

#endif
