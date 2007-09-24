#ifndef _RIFIUTI_H
#define _RIFIUTI_H

#include <stdint.h>
#include <time.h>

enum {
  LEGACY_FILENAME_OFFSET  = 0x4,
  RECORD_INDEX_OFFSET     = 0x108,
  DRIVE_LETTER_OFFSET     = 0x10C,
  FILETIME_OFFSET         = 0x110,
  FILESIZE_OFFSET         = 0x118,
  UNICODE_FILENAME_OFFSET = 0x11C
};

struct _rbin_struct {
  char      *legacy_filename;
  uint32_t   index;
  uint32_t   drive;
  int        emptied;
  struct tm *filetime;
  uint32_t   filesize;
  char      *utf8_filename;
};

typedef struct _rbin_struct rbin_struct;

#endif

/* vi: shiftwidth=2 expandtab tabstop=2
 */
