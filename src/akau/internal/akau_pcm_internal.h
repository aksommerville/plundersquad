#ifndef AKAU_PCM_INTERNAL_H
#define AKAU_PCM_INTERNAL_H

#include <stdint.h>
#include "../akau_pcm.h"

#define AKAU_PCM_COMMON_FIELDS \
  int refc; \
  int loopap,loopzp; \
  int lock; \
  int c;

struct akau_ipcm {
  AKAU_PCM_COMMON_FIELDS
  int16_t v[];
};

struct akau_fpcm {
  AKAU_PCM_COMMON_FIELDS
  double v[];
};

#endif
