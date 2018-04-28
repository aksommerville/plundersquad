#ifndef AKAU_INSTRUMENT_INTERNAL_H
#define AKAU_INSTRUMENT_INTERNAL_H

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "../akau_instrument.h"
#include "../akau_pcm.h"
#include "../akau_store.h"
#include "../akau_wavegen.h"

struct akau_instrument {
  int refc;
  struct akau_fpcm *fpcm;
  int attack_time;
  int drawback_time;
  int decay_time;
  double attack_trim;
  double drawback_trim;
  int fpcmid;
  struct akau_ipcm *ipcm;
};

#endif
