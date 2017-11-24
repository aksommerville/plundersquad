#include "akau_song_internal.h"

/* Evaluate signed integer with limits and logging.
 */

static int akau_song_decoder_eval_int(int *dst,struct akau_song_decoder *decoder,const char *src,int srcc,int lo,int hi,const char *reference) {
  if (!reference||!reference[0]) reference="integer";
  int srcp=0,positive=1;

  if (srcp>=srcc) return akau_song_decoder_error(decoder,"Found empty string, expected %s.",reference);
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return akau_song_decoder_error(decoder,"Found empty string, expected %s.",reference);
    positive=0;
  }

  *dst=0;
  while (srcp<srcc) {
    char digit=src[srcp++];
    if ((digit<'0')||(digit>'9')) return akau_song_decoder_error(decoder,"Illegal digit '%c' in %s.",digit,reference);
    digit-='0';
    if (positive) {
      if (*dst>INT_MAX/10) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s' as %s.",srcc,src,reference);
      *dst*=10;
      if (*dst>INT_MAX-digit) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s' as %s.",srcc,src,reference);
      *dst+=digit;
    } else {
      if (*dst<INT_MIN/10) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s' as %s.",srcc,src,reference);
      *dst*=10;
      if (*dst<INT_MIN+digit) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s' as %s.",srcc,src,reference);
      *dst-=digit;
    }
  }

  if (*dst<lo) {
    const char *qualifier=(lo<hi)?"at least":"exactly";
    return akau_song_decoder_error(decoder,"%s must be %s %d, found %d.",reference,qualifier,lo,*dst);
  } else if (*dst>hi) {
    const char *qualifier=(lo<hi)?"no more than":"exactly";
    return akau_song_decoder_error(decoder,"%s must be %s %d, found %d.",reference,qualifier,hi,*dst);
  } else {
    return 0;
  }
}

/* Measure and evaluate signed integer, no range check, return length consumed.
 */

static int akau_song_decoder_eval_int_tokenize(int *dst,struct akau_song_decoder *decoder,const char *src,int srcc) {
  int srcp=0,positive=1;

  if (srcp>=srcc) return 0;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return 0;
    positive=0;
  }
  if ((srcp>=srcc)||(src[srcp]<'0')||(src[srcp]>'9')) return 0;

  *dst=0;
  while (srcp<srcc) {
    char digit=src[srcp];
    if ((digit<'0')||(digit>'9')) break;
    digit-='0';
    if (positive) {
      if (*dst>INT_MAX/10) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s'.",srcc,src);
      *dst*=10;
      if (*dst>INT_MAX-digit) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s'.",srcc,src);
      *dst+=digit;
    } else {
      if (*dst<INT_MIN/10) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s'.",srcc,src);
      *dst*=10;
      if (*dst<INT_MIN+digit) return akau_song_decoder_error(decoder,"Integer overflow evaluating '%.*s'.",srcc,src);
      *dst-=digit;
    }
    srcp++;
  }

  return srcp;
}

/* Read the next line of text.
 * Advance (srcp) to the next newline.
 * Populate (wordv).
 * Automatically skip blank and comment lines.
 * Return >0 if populated, 0 at end of file, or <0 for real errors.
 */

static int akau_song_decoder_read_line(struct akau_song_decoder *decoder) {
  decoder->wordc=0;
  while (decoder->srcp<decoder->srcc) {

    /* Skip all whitespace. */
    if ((unsigned char)decoder->src[decoder->srcp]<=0x20) {
      decoder->srcp++;
      continue;
    }

    /* Full-line comments. */
    if (decoder->src[decoder->srcp]=='#') {
      while ((decoder->srcp<decoder->srcc)&&(decoder->src[decoder->srcp]!=0x0a)&&(decoder->src[decoder->srcp]!=0x0d)) {
        decoder->srcp++;
      }
      continue;
    }

    /* Section separator. Record a single word "-", skip the line, and terminate. */
    if (decoder->src[decoder->srcp]=='-') {
      if (akau_song_decoder_wordv_append(decoder,1)<0) return -1;
      while ((decoder->srcp<decoder->srcc)&&(decoder->src[decoder->srcp]!=0x0a)&&(decoder->src[decoder->srcp]!=0x0d)) {
        decoder->srcp++;
      }
      return 1;
    }

    /* Text line. Record words until newline. */
    while ((decoder->srcp<decoder->srcc)&&(decoder->src[decoder->srcp]!=0x0a)&&(decoder->src[decoder->srcp]!=0x0d)) {
      if ((unsigned char)decoder->src[decoder->srcp]<=0x20) {
        decoder->srcp++;
        continue;
      }
      const char *src=decoder->src+decoder->srcp;
      int srcc=1;
      while ((decoder->srcp+srcc<decoder->srcc)&&((unsigned char)src[srcc]>0x20)) srcc++;
      if (akau_song_decoder_wordv_append(decoder,srcc)<0) return -1;
      decoder->srcp+=srcc;
    }
    if (decoder->wordc<1) return -1; // Shouldn't be possible, but let's be sure.
    return 1;
  }
  return 0;
}

/* Assert word count.
 */

static int akau_song_decoder_assert_wordc(struct akau_song_decoder *decoder,int lo,int hi,const char *reference) {
  if (decoder->wordc<lo) {
    const char *description;
    if (lo<hi) description="at least";
    else description="exactly";
    if (reference) {
      return akau_song_decoder_error(decoder,"Require %s %d words for %s, found %d.",description,lo,reference,decoder->wordc);
    } else {
      return akau_song_decoder_error(decoder,"Expected %s %d words, found %d.",description,lo,decoder->wordc);
    }
  } else if (decoder->wordc>hi) {
    const char *description;
    if (lo<hi) description="no more than";
    else description="exactly";
    if (reference) {
      return akau_song_decoder_error(decoder,"Require %s %d words for %s, found %d.",description,hi,reference,decoder->wordc);
    } else {
      return akau_song_decoder_error(decoder,"Expected %s %d words, found %d.",description,hi,decoder->wordc);
    }
  }
  return 0;
}

/* Read word as unsigned integer.
 */

static int akau_song_decoder_uint_from_word(struct akau_song_decoder *decoder,int wordp,int lo,int hi,const char *reference) {
  if (akau_song_decoder_assert_wordc(decoder,wordp+1,INT_MAX,reference)<0) return -1;
  const char *src=decoder->wordv[wordp].v;
  int srcc=decoder->wordv[wordp].c;
  int dst=0,srcp=0;
  while (srcp<srcc) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) {
      return akau_song_decoder_error(decoder,"Unexpected character '%c' in integer.",src[srcp-1]);
    }
    if (dst>INT_MAX/10) return akau_song_decoder_error(decoder,"Integer overflow.");
    dst*=10;
    if (dst>INT_MAX-digit) return akau_song_decoder_error(decoder,"Integer overflow.");
    dst+=digit;
  }
  if (!reference||!reference[0]) reference="Integer";
  if (dst<lo) return akau_song_decoder_error(decoder,"%s must be at least %d, found %d.",reference,lo,dst);
  if (dst>hi) return akau_song_decoder_error(decoder,"%s must be no more than %d, found %d.",reference,hi,dst);
  return dst;
}

/* Definition: speed
 */

static int akau_song_decoder_decode_definitions_speed(struct akau_song_decoder *decoder) {

  if (akau_song_decoder_assert_wordc(decoder,2,2,"'speed'")<0) return -1;
  if ((decoder->speed=akau_song_decoder_uint_from_word(decoder,1,1,10000,"'speed'"))<0) return -1;
  
  int rate=akau_get_master_rate();
  if ((rate<1)||(rate>INT_MAX/60)) return akau_song_decoder_error(decoder,"Illegal global rate %d Hz.",rate);
  decoder->beat_delay=(rate*60)/decoder->speed;
  if (decoder->beat_delay<1) {
    return akau_song_decoder_error(decoder,"Speed %d bpm at output rate %d Hz yields illegal beat delay %d f.",decoder->speed,rate,decoder->beat_delay);
  }

  return 0;
}

/* Definition: instrument
 */

static int akau_song_decoder_decode_definitions_instrument(struct akau_song_decoder *decoder) {

  if ((decoder->wordc>=2)&&(decoder->wordv[1].c==8)&&!memcmp(decoder->wordv[1].v,"external",8)) {
    if (akau_song_decoder_assert_wordc(decoder,3,3,"external instrument reference")<0) return -1;
    int resid;
    if (akau_song_decoder_eval_int(&resid,decoder,decoder->wordv[2].v,decoder->wordv[2].c,1,INT_MAX,"instrument resource ID")<0) return -1;
    if (akau_song_add_external_instrument(decoder->song,resid)<0) return -1;
    return 0;
  }

  if (akau_song_decoder_assert_wordc(decoder,2,INT_MAX,"instrument definition")<0) return -1;
  const char *src=decoder->wordv[1].v;
  int srcc=decoder->wordv[decoder->wordc-1].v+decoder->wordv[decoder->wordc-1].c-src;
  struct akau_instrument *instrument=akau_instrument_decode(src,srcc);
  if (!instrument) {
    return akau_song_decoder_error(decoder,"Failed to decode instrument.");
  }

  if (akau_song_add_instrument(decoder->song,instrument)<0) {
    akau_instrument_del(instrument);
    return -1;
  }
  akau_instrument_del(instrument);
  return 0;
}

/* Definition: drum
 */

static int akau_song_decoder_decode_definitions_drum(struct akau_song_decoder *decoder) {

  if ((decoder->wordc>=2)&&(decoder->wordv[1].c==8)&&!memcmp(decoder->wordv[1].v,"external",8)) {
    if (akau_song_decoder_assert_wordc(decoder,3,3,"external drum reference")<0) return -1;
    int resid;
    if (akau_song_decoder_eval_int(&resid,decoder,decoder->wordv[2].v,decoder->wordv[2].c,1,INT_MAX,"ipcm resource ID")<0) return -1;
    if (akau_song_add_external_ipcm(decoder->song,resid)<0) return -1;
    return 0;
  }

  return akau_song_decoder_error(decoder,"Expected 'external' after 'drum' (inline definitions not supported).");
}

/* End of definitions section.
 */

static int akau_song_decoder_finish_definitions(struct akau_song_decoder *decoder) {
  if (decoder->speed<1) return akau_song_decoder_error(decoder,"Require 'speed' definition in beats per minute.");
  return 0;
}

/* Definitions section.
 */
 
int akau_song_decoder_decode_definitions(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  while (1) {
    int err=akau_song_decoder_read_line(decoder);
    if (err<0) return -1;
    if (!err) break;

    const struct akau_song_decoder_word *kw=decoder->wordv;
    if (kw->v[0]=='-') break;
    else if ((kw->c==5)&&!memcmp(kw->v,"speed",5)) {
      if (akau_song_decoder_decode_definitions_speed(decoder)<0) return -1;
    } else if ((kw->c==10)&&!memcmp(kw->v,"instrument",10)) {
      if (akau_song_decoder_decode_definitions_instrument(decoder)<0) return -1;
    } else if ((kw->c==4)&&!memcmp(kw->v,"drum",4)) {
      if (akau_song_decoder_decode_definitions_drum(decoder)<0) return -1;
    } else {
      return akau_song_decoder_error(decoder,"Unexpected keyword '%.*s' in definitions section.",kw->c,kw->v);
    }
  }
  return akau_song_decoder_finish_definitions(decoder);
}

/* Decode channels: helpers.
 */

static int akau_song_decoder_channels_is_null_word(const struct akau_song_decoder_word *word) {
  // Null word if empty or only dots.
  const char *v=word->v;
  int i=word->c; for (;i-->0;v++) {
    if (v[i]!='.') return 0;
  }
  return 1;
}

/* Decode channel, known field.
 */

static int akau_song_decoder_decode_channel_pan(struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc) {
  int pan;
  if (akau_song_decoder_eval_int(&pan,decoder,src,srcc,-128,127,"pan")<0) return -1;
  chan->pan_default=pan;
  return 0;
}

static int akau_song_decoder_decode_channel_trim(struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc) {
  int trim;
  if (akau_song_decoder_eval_int(&trim,decoder,src,srcc,0,255,"trim")<0) return -1;
  chan->trim_default=trim;
  return 0;
}

static int akau_song_decoder_decode_channel_instrument(struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc) {
  //TODO Should we permit instrument names?
  int instrid;
  if (akau_song_decoder_eval_int(&instrid,decoder,src,srcc,0,255,"instrument")<0) return -1;
  chan->instrument_default=instrid;
  return 0;
}

static int akau_song_decoder_decode_channel_drums(struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc) {
  if (src) return akau_song_decoder_error(decoder,"'drums' does not take an argument.");
  chan->drums=1;
  return 0;
}

/* Decode single channels entry.
 */

static int akau_song_decoder_decode_channel_header(
  struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const struct akau_song_decoder_word *word,int chanid
) {
  if (akau_song_decoder_channels_is_null_word(word)) return 0;

  /* Split key and value. */
  const char *k=word->v,*v=0;
  int kc=0,vc=0;
  while ((kc<word->c)&&(k[kc]!=':')) kc++;
  if (kc<word->c) {
    v=k+kc+1;
    vc=word->c-kc-1;
  }

  /* Dispatch on key. */
  if ((kc==3)&&!memcmp(k,"pan",3)) return akau_song_decoder_decode_channel_pan(decoder,chan,v,vc);
  if ((kc==4)&&!memcmp(k,"trim",4)) return akau_song_decoder_decode_channel_trim(decoder,chan,v,vc);
  if ((kc>=3)&&(kc<=10)&&!memcmp(k,"instrument",kc)) return akau_song_decoder_decode_channel_instrument(decoder,chan,v,vc);
  if ((kc==5)&&!memcmp(k,"drums",5)) return akau_song_decoder_decode_channel_drums(decoder,chan,v,vc);

  return akau_song_decoder_error(decoder,"Unexpected keyword '%.*s' for channel %d.",kc,k,chanid);
}

/* Finish channels section.
 */

static int akau_song_decoder_finish_channels(struct akau_song_decoder *decoder) {
  if (decoder->chanc<1) return akau_song_decoder_error(decoder,"No channels defined.");
  return 0;
}

/* Channels section.
 */
 
int akau_song_decoder_decode_channels(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  while (1) {
    int err=akau_song_decoder_read_line(decoder);
    if (err<0) return -1;
    if (!err) break;

    const struct akau_song_decoder_word *kw=decoder->wordv;
    if (kw->v[0]=='-') {
      break;
    } else {

      if (decoder->chanc<1) {
        if (akau_song_decoder_set_chanc(decoder,decoder->wordc)<0) return -1;
      }
      
      if (akau_song_decoder_assert_wordc(decoder,1,decoder->chanc,"channel definition")<0) return -1;
      struct akau_song_decoder_chan *chan=decoder->chanv;
      struct akau_song_decoder_word *word=decoder->wordv;
      int i; for (i=0;i<decoder->wordc;i++,chan++,word++) {
        if (akau_song_decoder_decode_channel_header(decoder,chan,word,i)<0) {
          return akau_song_decoder_error(decoder,"Failed to decode '%.*s' for channel %d.",word->c,word->v,i);
        }
      }
    }
  }
  return akau_song_decoder_finish_channels(decoder);
}

/* Evaluate trim. Prepopulate with the channel default.
 */

static int akau_song_decoder_eval_trim(uint8_t *dst,struct akau_song_decoder *decoder,const char *src,int srcc) {
  int srcp=1,n,err;
  if ((srcc<1)||(src[0]!=':')) return 0;
  if ((err=akau_song_decoder_eval_int_tokenize(&n,decoder,src+srcp,srcc-srcp))<1) {
    return akau_song_decoder_error(decoder,"Expected trim after ':'.");
  }
  srcp+=err;
  if ((n<0)||(n>255)) return akau_song_decoder_error(decoder,"Illegal trim %d (0..255).",n);
  if ((srcp<srcc)&&(src[srcp]=='%')) {
    *dst=(n*(*dst))/100;
    srcp++;
  } else {
    *dst=n;
  }
  return srcp;
}

/* Evaluate pan. Prepopulate with the channel default.
 */

static int akau_song_decoder_eval_pan(int8_t *dst,struct akau_song_decoder *decoder,const char *src,int srcc) {
  int srcp=1,n,err;
  if ((srcc<1)||(src[0]!='@')) return 0;
  if ((err=akau_song_decoder_eval_int_tokenize(&n,decoder,src+srcp,srcc-srcp))<1) {
    return akau_song_decoder_error(decoder,"Expected pan after '@'.");
  }
  srcp+=err;
  if ((n<-128)||(n>127)) return akau_song_decoder_error(decoder,"Illegal pan %d (-128..127).",n);
  if ((srcp<srcc)&&(src[srcp]=='~')) {
    n+=*dst;
    if (n<=-128) *dst=-128;
    else if (n>=127) *dst=127;
    else *dst=n;
    srcp++;
  } else {
    *dst=n;
  }
  return srcp;
}

/* Evaluate pitch.
 */

static int akau_song_decoder_decode_pitch(uint8_t *dst,struct akau_song_decoder *decoder,const char *src,int srcc) {
  int srcp=0;
  
  if (srcp>=srcc) return -1;
  char name=src[srcp++];
  if ((name>='A')&&(name<='Z')) name+=0x20;
  if ((name<'a')||(name>'g')) {
    return akau_song_decoder_error(decoder,"Expected note name (a..g), found '%c'.",name);
  }

  int modifier=0;
  if (srcp>=srcc) return -1;
  if (src[srcp]=='b') {
    modifier=-1;
    srcp++;
  } else if (src[srcp]=='#') {
    modifier=1;
    srcp++;
  }

  int octave=0;
  if ((srcp>=srcc)||(src[srcp]<'0')||(src[srcp]>'9')) return -1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    octave*=10;
    octave+=digit;
    if (octave>99) return akau_song_decoder_error(decoder,"Note octave too large.");
  }

  if (srcp<srcc) {
    if (
      ((src[srcp]>='a')&&(src[srcp]<='z'))||
      ((src[srcp]>='A')&&(src[srcp]<='Z'))
    ) return akau_song_decoder_error(decoder,"Unexpected characters after note '%.*s'.",srcp,src);
  }

  if ((name=='a')&&(modifier<0)&&(octave==0)) {
    return akau_song_decoder_error(decoder,"a0 is the lowest note, it can't be flatted.");
  }

  switch (name) {
    case 'a': *dst= 0+modifier; break;
    case 'b': *dst= 2+modifier; break;
    case 'c': *dst= 3+modifier; break;
    case 'd': *dst= 5+modifier; break;
    case 'e': *dst= 7+modifier; break;
    case 'f': *dst= 8+modifier; break;
    case 'g': *dst=10+modifier; break;
    default: return -1;
  }

  octave*=12;
  if (*dst>255-octave) return akau_song_decoder_error(decoder,"Note '%.*s' too high (limit c21).",srcp,srcc);
  *dst+=octave;

  return srcp;
}

/* Decode one word of body.
 */

static int akau_song_decoder_decode_body_word_drums(
  struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc
) {
  int srcp=0,n,err;
  uint8_t drumid=0;
  uint8_t trim=chan->trim_default;
  int8_t pan=chan->pan_default;

  /* Drum ID. */
  if ((err=akau_song_decoder_eval_int_tokenize(&n,decoder,src+srcp,srcc-srcp))<1) {
    return akau_song_decoder_error(decoder,"Expected drum ID, found '%.*s'.",srcc,src);
  }
  srcp+=err;
  if ((n<0)||(n>=decoder->song->ipcmc)) {
    if (!decoder->song->ipcmc) return akau_song_decoder_error(decoder,"No drums defined.");
    return akau_song_decoder_error(decoder,"Undefined drum ID %d (0..%d)",n,decoder->song->ipcmc-1);
  }
  drumid=n;

  /* Trim, pan. */
  if ((err=akau_song_decoder_eval_trim(&trim,decoder,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;
  if ((err=akau_song_decoder_eval_pan(&pan,decoder,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;

  if (srcp<srcc) return akau_song_decoder_error(decoder,"Unexpected extra tokens in drum command: '%.*s'",srcc-srcp,src+srcp);
  if (akau_song_addcmd_drum(decoder->song,drumid,trim,pan)<0) return -1;

  return 0;
}

static int akau_song_decoder_decode_body_word_instrument(
  struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const char *src,int srcc
) {
  int srcp=0,n,err;
  uint8_t instrumentid=chan->instrument_default;
  uint8_t pitch;
  int duration=decoder->beat_delay;
  uint8_t trim=chan->trim_default;
  int8_t pan=chan->pan_default;

  /* Instrument ID. */
  if ((srcp<srcc)&&(src[srcp]=='(')) {
    srcp++;
    if ((err=akau_song_decoder_eval_int_tokenize(&n,decoder,src+srcp,srcc-srcp))<1) {
      return akau_song_decoder_error(decoder,"Expected instrument ID before '%.*s'.",srcc-srcp,src+srcp);
    }
    srcp+=err;
    if ((srcp>=srcc)||(src[srcp]!=')')) {
      return akau_song_decoder_error(decoder,"Expected ')' before '%.*s'.",srcc-srcp,src+srcp);
    }
    srcp++;
    if ((n<0)||(n>=decoder->song->instrumentc)) {
      if (!decoder->song->instrumentc) return akau_song_decoder_error(decoder,"No instruments defined.");
      return akau_song_decoder_error(decoder,"Undefined instrument ID %d (0..%d).",n,decoder->song->instrumentc-1);
    }
    instrumentid=n;
  }

  /* Pitch. */
  if ((err=akau_song_decoder_decode_pitch(&pitch,decoder,src+srcp,srcc-srcp))<1) {
    return akau_song_decoder_error(decoder,"Failed to decode pitch in note '%.*s'.",srcc,src);
  }
  srcp+=err;

  /* Duration. */
  if ((srcp<srcc)&&(src[srcp]=='=')) {
    srcp++;
    if ((err=akau_song_decoder_eval_int_tokenize(&duration,decoder,src+srcp,srcc-srcp))<1) {
      return akau_song_decoder_error(decoder,"Expected duration after '=', found '%.*s'.",srcc-srcp,src+srcp);
    }
    srcp+=err;
    if (duration<1) {
      return akau_song_decoder_error(decoder,"Invalid note duration %d.",duration);
    }
    if (duration>INT_MAX/decoder->beat_delay) {
      return akau_song_decoder_error(decoder,"Note duration too long (%d, limit %d for this speed and output rate).",duration,INT_MAX/decoder->beat_delay);
    }
    duration*=decoder->beat_delay;
  }

  /* Trim, pan. */
  if ((err=akau_song_decoder_eval_trim(&trim,decoder,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;
  if ((err=akau_song_decoder_eval_pan(&pan,decoder,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;

  if (srcp<srcc) return akau_song_decoder_error(decoder,"Unexpected extra tokens in instrument command: '%.*s'",srcc-srcp,src+srcp);

  if (akau_song_addcmd_note(decoder->song,instrumentid,pitch,trim,pan,duration)<0) return -1;
  
  return 0;
}

static int akau_song_decoder_decode_body_word(
  struct akau_song_decoder *decoder,struct akau_song_decoder_chan *chan,const struct akau_song_decoder_word *word
) {

  if (word->c<1) return 0;
  if ((word->c==1)&&(word->v[0]=='.')) return 0;

  if (chan->drums) {
    return akau_song_decoder_decode_body_word_drums(decoder,chan,word->v,word->c);
  } else {
    return akau_song_decoder_decode_body_word_instrument(decoder,chan,word->v,word->c);
  }
}

/* Decode synchronization command.
 */

static int akau_song_decoder_decode_body_sync(struct akau_song_decoder *decoder,const char *src,int srcc) {
  int token;
  if (akau_song_decoder_eval_int(&token,decoder,src,srcc,0,0xffff,"sync token")<0) return -1;
  if (akau_song_addcmd_sync(decoder->song,token)<0) return -1;
  return 0;
}

/* Decode one line of body.
 */

static int akau_song_decoder_decode_body_line(struct akau_song_decoder *decoder) {

  if ((decoder->wordc==2)&&(decoder->wordv[0].c==4)&&!memcmp(decoder->wordv[0].v,"sync",4)) {
    if (akau_song_decoder_decode_body_sync(decoder,decoder->wordv[1].v,decoder->wordv[1].c)<0) return -1;
    return 0;
  }

  if (akau_song_decoder_assert_wordc(decoder,1,decoder->chanc,"beat")<0) return -1;
  
  struct akau_song_decoder_chan *chan=decoder->chanv;
  const struct akau_song_decoder_word *word=decoder->wordv;
  int i=decoder->wordc; for (;i-->0;chan++,word++) {
    if (akau_song_decoder_decode_body_word(decoder,chan,word)<0) {
      return akau_song_decoder_error(decoder,"Failed to decode word '%.*s' in note data.",word->c,word->v);
    }
  }

  if (akau_song_addcmd_delay(decoder->song,decoder->beat_delay)<0) return -1;
  
  return 0;
}

/* Finish body.
 */

static int akau_song_decoder_finish_body(struct akau_song_decoder *decoder) {
  return 0;
}

/* Body section.
 */
 
int akau_song_decoder_decode_body(struct akau_song_decoder *decoder) {
  if (!decoder) return -1;
  while (1) {
    int err=akau_song_decoder_read_line(decoder);
    if (err<0) return -1;
    if (!err) break;

    if (akau_song_decoder_decode_body_line(decoder)<0) return -1;
  }
  return akau_song_decoder_finish_body(decoder);
}
