#ifndef AKAU_MIXER_INTERNAL_H
#define AKAU_MIXER_INTERNAL_H

#include "../akau_mixer.h"
#include "../akau_wavegen.h"
#include "../akau_song.h"
#include "akau_instrument_internal.h"
#include "akau_pcm_internal.h"
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#define AKAU_MIXER_CHAN_MODE_SILENT     0
#define AKAU_MIXER_CHAN_MODE_TUNED      1
#define AKAU_MIXER_CHAN_MODE_VERBATIM   2

#define AKAU_MIXER_TUNED_PHASE_ATTACK   1
#define AKAU_MIXER_TUNED_PHASE_DRAWBACK 2
#define AKAU_MIXER_TUNED_PHASE_LOOP     3
#define AKAU_MIXER_TUNED_PHASE_DECAY    4

struct akau_mixer_chan {
  int chanid;
  int mode;
  int stop_at_silence; // If trim reaches zero, wipe out this channel.
  uint8_t intent;
  
  uint8_t trim;
  uint8_t trima,trimz;
  int trimp,trimc;
  
  int8_t pan;
  int8_t pana,panz;
  int panp,panc;
  
  union {

    struct {
      struct akau_instrument *instrument;
      double p;
      
      double dp;
      double dpa,dpz;
      int dpp,dpc;

      uint8_t pitch; // Advisory only.
      int phase;
      int phasep,phasec;

      int ttl;
      
    } tuned;

    struct {
      struct akau_ipcm *ipcm;
      int p;
      int loop;
    } verbatim;
    
  };
};

struct akau_mixer {
  int refc;
  struct akau_mixer_chan *chanv;
  int chanc,chana;
  int chanid_next;
  int cliplc,cliprc;
  struct akau_song *song;
  int song_delay;
  int song_cmdp;
  uint8_t song_intent;
  uint8_t trim_by_intent[256];
};

void akau_mixer_chan_cleanup(struct akau_mixer_chan *chan);

int akau_mixer_chanv_realloc(struct akau_mixer *mixer);
int akau_mixer_chanv_search(const struct akau_mixer *mixer,int chanid);

#endif
