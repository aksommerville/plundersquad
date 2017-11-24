#include "akau_song_internal.h"
#include <stdarg.h>
#include <stdio.h>

/* Decoder lifecycle.
 */

struct akau_song_decoder *akau_song_decoder_new() {
  struct akau_song_decoder *decoder=calloc(1,sizeof(struct akau_song_decoder));
  if (!decoder) return 0;

  decoder->refc=1;

  return decoder;
}

void akau_song_decoder_del(struct akau_song_decoder *decoder) {
  if (!decoder) return;
  if (decoder->refc-->1) return;

  akau_song_del(decoder->song);
  if (decoder->chanv) free(decoder->chanv);
  if (decoder->wordv) free(decoder->wordv);

  free(decoder);
}

int akau_song_decoder_ref(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  if (decoder->refc<1) return -1;
  if (decoder->refc==INT_MAX) return -1;
  decoder->refc++;
  return 0;
}

/* Reset.
 */

int akau_song_decoder_reset(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  if (decoder->song) {
    akau_song_del(decoder->song);
    decoder->song=0;
  }
  decoder->src=0;
  decoder->srcc=0;
  decoder->srcp=0;
  decoder->cb_error=0;
  decoder->chanc=0;
  decoder->wordc=0;
  return 0;
}

/* Set channel count.
 */

int akau_song_decoder_set_chanc(struct akau_song_decoder *decoder,int nchanc) {
  if (!decoder) return -1;
  if (nchanc<1) return -1;
  if (nchanc>decoder->chana) {
    if (nchanc>INT_MAX/sizeof(struct akau_song_decoder_chan)) return -1;
    void *nv=realloc(decoder->chanv,sizeof(struct akau_song_decoder_chan)*nchanc);
    if (!nv) return -1;
    decoder->chanv=nv;
    decoder->chana=nchanc;
  }
  decoder->chanc=nchanc;
  memset(decoder->chanv,0,sizeof(struct akau_song_decoder_chan)*nchanc);
  struct akau_song_decoder_chan *chan=decoder->chanv;
  int i=decoder->chanc; for (;i-->0;chan++) {
    chan->trim_default=0xff;
  }
  return 0;
}

/* Grow word list.
 */

int akau_song_decoder_wordv_require(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  if (decoder->wordc<decoder->worda) return 0;
  int na=decoder->worda+8;
  if (na>INT_MAX/sizeof(struct akau_song_decoder_word)) return -1;
  void *nv=realloc(decoder->wordv,sizeof(struct akau_song_decoder_word)*na);
  if (!nv) return -1;
  decoder->wordv=nv;
  decoder->worda=na;
  return 0;
}

/* Add word to list.
 */
 
int akau_song_decoder_wordv_append(struct akau_song_decoder *decoder,int c) {
  if (akau_song_decoder_wordv_require(decoder)<0) return -1;
  if (c<1) return -1;
  struct akau_song_decoder_word *word=decoder->wordv+decoder->wordc++;
  word->v=decoder->src+decoder->srcp;
  word->p=decoder->srcp;
  word->c=c;
  return 0;
}

/* Report error.
 */
 
int akau_song_decoder_error(struct akau_song_decoder *decoder,const char *fmt,...) {
  if (!decoder) return -1;
  if (decoder->cb_error) {
    if (!fmt) fmt="";
    va_list vargs;
    va_start(vargs,fmt);
    char buf[256];
    int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
    if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
    decoder->cb_error(buf,bufc,decoder->src,decoder->srcc,decoder->srcp);
    decoder->cb_error=0;
  }
  return -1;
}

/* Finish decoding.
 */

static int akau_song_decoder_finish(struct akau_song_decoder *decoder) {
  if (decoder->song->cmdc<1) {
    return akau_song_decoder_error(decoder,"No body content.");
  }
  return 0;
}

/* Decode, main entry point.
 */

struct akau_song *akau_song_decoder_decode(
  struct akau_song_decoder *decoder,
  const char *src,int srcc,
  void (*cb_error)(const char *msg,int msgc,const char *src,int srcc,int srcp)
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (akau_song_decoder_reset(decoder)<0) return 0;

  if (!(decoder->song=akau_song_new())) return 0;
  decoder->src=src;
  decoder->srcc=srcc;
  decoder->cb_error=cb_error;

  if (akau_song_decoder_decode_definitions(decoder)<0) {
    akau_song_decoder_error(decoder,"Failed to decode definitions.");
    return 0;
  }
  if (akau_song_decoder_decode_channels(decoder)<0) {
    akau_song_decoder_error(decoder,"Failed to decode channels.");
    return 0;
  }
  if (akau_song_decoder_decode_body(decoder)<0) {
    akau_song_decoder_error(decoder,"Failed to decode body.");
    return 0;
  }
  if (akau_song_decoder_finish(decoder)<0) {
    akau_song_decoder_error(decoder,"Failed to finalize song.");
    return 0;
  }

  if (akau_song_ref(decoder->song)<0) {
    struct akau_song *song=decoder->song;
    decoder->song=0;
    return song;
  } else {
    return decoder->song;
  }
}
