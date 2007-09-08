#ifndef _RIFIUTI_H
#define _RIFIUTI_H

#include <stdint.h>

enum {
  RIFIUTI_ERR_ARG = 1,
  RIFIUTI_ERR_OPEN_FILE,
  RIFIUTI_ERR_BROKEN_FILE
};

enum {
  LEGACY_FILENAME_OFFSET  = 0x4,
  RECORD_INDEX_OFFSET     = 0x108,
  DRIVE_LETTER_OFFSET     = 0x10C,
  FILETIME_OFFSET         = 0x110,
  FILESIZE_OFFSET         = 0x118,
  UNICODE_FILENAME_OFFSET = 0x11C
};

struct _info2 {
  char     *legacy_filename;
  uint32_t  record_index;
  uint32_t  drive_letter;
  uint64_t  filetime;
  uint32_t  filesize;
  char     *utf8_filename;
};

typedef struct _info2 info2;

#endif

/* vi: shiftwidth=2 expandtab tabstop=2
 */
