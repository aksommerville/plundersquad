#ifndef AKAU_WAVEGEN_INTERNAL_H
#define AKAU_WAVEGEN_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "akau_internal.h"
#include "akau_pcm_internal.h"
#include "../akau_wavegen.h"

#include <stdio.h>

#define AKAU_A0_FREQ 27.5

struct akau_wavegen_chan {

  struct akau_fpcm *fpcm;
  double samplep;

  double trim;
  double trima,trimz;
  int trimap,trimzp;

  double step;
  double stepa,stepz;
  int stepap,stepzp;

};

struct akau_wavegen_cmd {
  int framep;
  int chanid;
  int k;
  double v;
};

struct akau_wavegen_decoder {
  const char *src;
  int srcc;
  int srcp;
  
  struct akau_wavegen_chan *chanv;
  int chanc,chana;
  int framep;

  struct akau_wavegen_cmd *cmdv;
  int cmdc,cmda;
  int cmdp;
};

#endif
