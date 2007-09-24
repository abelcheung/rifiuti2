#include "utils.h"

time_t win_filetime_to_epoch (uint64_t win_filetime)
{
  uint64_t epoch;

  /* I suppose millisecond resolution is not needed? -- Abel */
  epoch = (win_filetime - 116444736000000000LL) / 10000000;

  /* Will it go wrong? Hope not. */
  return (time_t) (epoch & 0xFFFFFFFF);
}

