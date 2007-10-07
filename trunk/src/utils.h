#ifndef _UTILS_H
#define _UTILS_H

#include <inttypes.h>
#include <time.h>

enum {
  RIFIUTI_ERR_ARG = 1,
  RIFIUTI_ERR_OPEN_FILE,
  RIFIUTI_ERR_BROKEN_FILE
};

enum {
  OUTPUT_CSV,
  OUTPUT_XML
};

time_t win_filetime_to_epoch (uint64_t win_filetime);

#endif
