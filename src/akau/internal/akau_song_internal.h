#ifndef AKAU_SONG_INTERNAL_H
#define AKAU_SONG_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "akau_internal.h"
#include "../akau_song.h"

struct akau_song_drum {
  int ipcmid;
  struct akau_ipcm *ipcm;
};

struct akau_song_instrument {
  void *serial;
  int serialc;
  struct akau_instrument *instrument;
};

struct akau_song {
  int refc;
  int lock;
  uint16_t tempo;
  int frames_per_beat; // Transient; changes with tempo.
  struct akau_song_drum *drumv;
  int drumc,druma;
  struct akau_song_instrument *instrv;
  int instrc,instra;
  union akau_song_command *cmdv;
  int cmdc,cmda;
  int chanid_ref[256];
};

#endif
