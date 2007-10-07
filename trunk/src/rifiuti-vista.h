#ifndef _RIFIUTI_VISTA_H
#define _RIFIUTI_VISTA_H

#include <inttypes.h>
#include <time.h>

enum {
  FILESIZE_OFFSET = 0x8,
  FILETIME_OFFSET = 0x10,
  FILENAME_OFFSET = 0x18
};

struct _rbin_struct {
  uint64_t   filesize;
  struct tm *filetime;
  char      *utf8_filename;
};

typedef struct _rbin_struct rbin_struct;

#endif

/* vi: shiftwidth=2 expandtab tabstop=2
 */
