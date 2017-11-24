#ifndef AKAU_SONG_INTERNAL_H
#define AKAU_SONG_INTERNAL_H

/* TODO: Can we find some way to modify notes in-flight?
 * I'm thinking of glissando and LFO.
 * Maybe add a token to NOTE and DRUM commands, that we can refer to in subsequent commands?
 * To keep it simple, I'm writing it first without any in-flight adjustment.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "akau_internal.h"
#include "../akau_song.h"
#include "../akau_instrument.h"
#include "../akau_pcm.h"
#include "../akau_mixer.h"

#define AKAU_SONG_OP_NOOP        0
#define AKAU_SONG_OP_DELAY       1
#define AKAU_SONG_OP_SYNC        2
#define AKAU_SONG_OP_NOTE        3
#define AKAU_SONG_OP_DRUM        4

struct akau_song_cmd {
  union {
    uint8_t op;
    struct { uint8_t op; uint16_t framec; } DELAY;
    struct { uint8_t op; uint16_t token; } SYNC;
    struct { uint8_t op; uint8_t instrid,pitch,trim,pan; int32_t framec; } NOTE;
    struct { uint8_t op; uint8_t ipcmid,trim,pan; } DRUM;
  };
};

struct akau_song_reference {
  int type; // Same as store restype: 1=ipcm, 3=instrument
  int internal_id; // Index in our local instrument or ipcm list.
  int external_id; // Resource ID in store.
};

struct akau_song {
  int refc;
  int lock;
  struct akau_instrument **instrumentv;
  int instrumentc,instrumenta;
  struct akau_ipcm **ipcmv;
  int ipcmc,ipcma;
  struct akau_song_cmd *cmdv;
  int cmdc,cmda;
  struct akau_song_reference *referencev; // Depopulated at link.
  int referencec,referencea;
};

struct akau_song_cmd *akau_song_addcmd(struct akau_song *song,uint8_t op);

struct akau_song_decoder_chan {
  uint8_t instrument_default;
  uint8_t trim_default;
  int8_t pan_default;
  uint8_t drums; // boolean
};

struct akau_song_decoder_word {
  const char *v; // (src+srcp+p), for convenience
  int c;
  int p;
};

struct akau_song_decoder {
  int refc;
  struct akau_song *song;
  const char *src;
  int srcc;
  int srcp;
  void (*cb_error)(const char *msg,int msgc,const char *src,int srcc,int srcp);
  struct akau_song_decoder_chan *chanv;
  int chanc,chana;
  struct akau_song_decoder_word *wordv;
  int wordc,worda;
  int speed;
  int beat_delay;
};

int akau_song_decoder_reset(struct akau_song_decoder *decoder);

int akau_song_decoder_set_chanc(struct akau_song_decoder *decoder,int nchanc);
int akau_song_decoder_wordv_require(struct akau_song_decoder *decoder); // Require at least one.
int akau_song_decoder_pointv_require(struct akau_song_decoder *decoder); // Require at least (chanc).
int akau_song_decoder_wordv_append(struct akau_song_decoder *decoder,int c); // Add a word at the current position.

int akau_song_decoder_decode_definitions(struct akau_song_decoder *decoder);
int akau_song_decoder_decode_channels(struct akau_song_decoder *decoder);
int akau_song_decoder_decode_body(struct akau_song_decoder *decoder);

/* Deliver an optional error message.
 * Clears (decoder->cb_error), ensuring that only one message will be delivered.
 * Always returns -1, for convenience.
 */
int akau_song_decoder_error(struct akau_song_decoder *decoder,const char *fmt,...);

#endif
