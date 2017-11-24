#include "akau_wavegen_internal.h"

/* Measure and evaluate positive decimal integer. Consume leading and trailing whitespace.
 */

static int akau_wavegen_decode_int(int *dst,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp]<'0')||(src[srcp]>'9')) return -1;
  *dst=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    if (*dst>INT_MAX/10) return -1;
    *dst*=10;
    if (*dst>INT_MAX-digit) return -1;
    *dst+=digit;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

/* Measure and evaluate float. Consume leading and trailing whitespace.
 */

static int akau_wavegen_decode_float(double *dst,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  int positive=1;
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
  
  *dst=0.0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    *dst*=10.0;
    *dst+=digit;
  }

  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    double coef=1.0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      coef*=0.1;
      *dst+=digit*coef;
    }
  }

  if (!positive) *dst=-*dst;
  
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

/* Measure and evaluate command key. Consume leading and trailing whitespace.
 */

static int akau_wavegen_decode_key(int *dst,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

  const char *k=src+srcp;
  int kc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kc++; }
  if ((kc==4)&&!memcmp(k,"noop",4)) *dst=AKAU_WAVEGEN_K_NOOP;
  else if ((kc==4)&&!memcmp(k,"step",4)) *dst=AKAU_WAVEGEN_K_STEP;
  else if ((kc==4)&&!memcmp(k,"trim",4)) *dst=AKAU_WAVEGEN_K_TRIM;
  else return akau_error("Expected (noop,stem,trim), found '%.*s'.",kc,k);
  
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

/* Decode command.
 */

static int akau_wavegen_decode_command(struct akau_wavegen_decoder *decoder,const char *src,int srcc) {
  int srcp=0,err,time,chanid,k;
  double v;

  if ((err=akau_wavegen_decode_int(&time,src+srcp,srcc-srcp))<1) return -1; srcp+=err;
  if ((err=akau_wavegen_decode_int(&chanid,src+srcp,srcc-srcp))<1) return -1; srcp+=err;
  if ((err=akau_wavegen_decode_key(&k,src+srcp,srcc-srcp))<1) return -1; srcp+=err;
  if ((err=akau_wavegen_decode_float(&v,src+srcp,srcc-srcp))<1) return -1; srcp+=err;
  if (srcp<srcc) return akau_error("Unexpected tokens after wavegen command: %.*s",srcc-srcp,src+srcp);

  // Considering int overflow, we can go about 48 seconds at 44100 Hz. 
  // That's plenty for my intended use case (foley).
  int rate=akau_get_master_rate();
  if (time>INT_MAX/rate) return akau_error("Time %d is too long for output rate %d.",time,rate);
  int framep=(time*rate)/1000;

  if (akau_wavegen_decoder_add_command(decoder,framep,chanid,k,v)<0) return -1;

  return 0;
}

/* Decode channel definition.
 * The introducer "channel" is already skipped.
 */

// We will delete fpcm.
static int akau_wavegen_decode_channel_1(struct akau_wavegen_decoder *decoder,struct akau_fpcm *fpcm) {
  int err=akau_wavegen_decoder_add_channel(decoder,fpcm);
  akau_fpcm_del(fpcm);
  return err;
}

static int akau_wavegen_decode_channel(struct akau_wavegen_decoder *decoder,const char *src,int srcc) {

  /* Check for macros. */
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int kwc=srcc-srcp;
  if ((kwc==4)&&!memcmp(src+srcp,"sine",4)) return akau_wavegen_decoder_add_channel(decoder,akau_wavegen_get_shared_sine());
  if ((kwc==6)&&!memcmp(src+srcp,"square",6)) return akau_wavegen_decode_channel_1(decoder,akau_generate_fpcm_square(akau_get_master_rate(),0));
  if ((kwc==3)&&!memcmp(src+srcp,"saw",3)) return akau_wavegen_decode_channel_1(decoder,akau_generate_fpcm_saw(akau_get_master_rate()));
  if ((kwc==5)&&!memcmp(src+srcp,"noise",5)) return akau_wavegen_decode_channel_1(decoder,akau_generate_fpcm_whitenoise(akau_get_master_rate()));

  /* Assume it's the wavegen harmonics format. */
  return akau_wavegen_decode_channel_1(decoder,akau_decode_fpcm_harmonics(0,src,srcc,1));
}

/* Decode line.
 */

static int akau_wavegen_decode_line(struct akau_wavegen_decoder *decoder,const char *src,int srcc) {

  /* Read the first word. If "channel", this is a channel declaration. */
  const char *kw=src;
  int kwc=0,srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }
  if ((kwc==7)&&!memcmp(kw,"channel",7)) {
    return akau_wavegen_decode_channel(decoder,src+srcp,srcc-srcp);
  }

  /* If it's not a channel definition, it's a command. */
  return akau_wavegen_decode_command(decoder,src,srcc);
}

/* Decode, protected entry point.
 */

static int akau_wavegen_decoder_decode_1(struct akau_wavegen_decoder *decoder) {
  while (decoder->srcp<decoder->srcc) {

    /* Skip leading whitespace (and newlines, blank lines, etc) */
    if ((unsigned char)decoder->src[decoder->srcp]<=0x20) {
      decoder->srcp++;
      continue;
    }

    /* Measure next line, stripping comment. */
    const char *line=decoder->src+decoder->srcp;
    int linec=0,cmt=0;
    while ((decoder->srcp<decoder->srcc)&&(decoder->src[decoder->srcp]!=0x0a)&&(decoder->src[decoder->srcp]!=0x0d)) {
      if (decoder->src[decoder->srcp]=='#') {
        cmt=1;
      } else if (!cmt) {
        linec++;
      }
      decoder->srcp++;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;

    /* Process single line. */
    if (!linec) continue;
    if (akau_wavegen_decode_line(decoder,line,linec)<0) return -1;
  }
  return 0;
}

/* Insert channel initialization commands if necessary.
 */

static int akau_wavegen_initialize_channel(struct akau_wavegen_decoder *decoder,int chanid) {
  int need_step=1,need_trim=1,i;
  for (i=0;i<decoder->cmdc;i++) {
    if (decoder->cmdv[i].framep) break;
    if (decoder->cmdv[i].chanid!=chanid) continue;
    switch (decoder->cmdv[i].k) {
      case AKAU_WAVEGEN_K_TRIM: need_trim=0; break;
      case AKAU_WAVEGEN_K_STEP: need_step=0; break;
    }
  }
  if (need_step) {
    if (akau_wavegen_decoder_add_command(decoder,0,chanid,AKAU_WAVEGEN_K_STEP,1.0)<0) return -1;
  }
  if (need_trim) {
    if (akau_wavegen_decoder_add_command(decoder,0,chanid,AKAU_WAVEGEN_K_TRIM,0.0)<0) return -1;
  }
  return 0;
}

static int akau_wavegen_terminate_channel(struct akau_wavegen_decoder *decoder,int chanid) {
  int framep=decoder->cmdv[decoder->cmdc-1].framep;
  if (akau_wavegen_decoder_add_command(decoder,framep,chanid,AKAU_WAVEGEN_K_TRIM,0.0)<0) return -1;
  return 0;
}

/* Finish decoding.
 */

static int akau_wavegen_decode_finish(struct akau_wavegen_decoder *decoder) {

  if (decoder->cmdc<1) return akau_error("No commands in WaveGen.");
  if (decoder->cmdv[decoder->cmdc-1].framep<1) return akau_error("WaveGen output length zero.");

  int chanid;
  for (chanid=0;chanid<decoder->chanc;chanid++) {
    if (akau_wavegen_initialize_channel(decoder,chanid)<0) return -1;
    if (akau_wavegen_terminate_channel(decoder,chanid)<0) return -1;
  }

  const struct akau_wavegen_cmd *cmd=decoder->cmdv;
  int i=decoder->cmdc; for (;i-->0;cmd++) {
    if ((cmd->chanid<0)||(cmd->chanid>=decoder->chanc)) {
      return akau_error("WaveGen command refers to nonexistant channel %d.",cmd->chanid);
    }
  }
  
  return 0;
}

/* Decode, main entry point.
 */

int akau_wavegen_decoder_decode(struct akau_wavegen_decoder *decoder,const char *src,int srcc) {
  if (!decoder) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  while (decoder->chanc>0) {
    decoder->chanc--;
    struct akau_wavegen_chan *chan=decoder->chanv+decoder->chanc;
    akau_fpcm_del(chan->fpcm);
  }
  decoder->cmdc=0;
  decoder->src=src;
  decoder->srcc=srcc;
  decoder->srcp=0;
  decoder->framep=0;
  decoder->cmdp=0;

  int err=akau_wavegen_decoder_decode_1(decoder);
  decoder->src=0;
  decoder->srcc=0;
  decoder->srcp=0;
  if (err<0) return err;

  if (akau_wavegen_decode_finish(decoder)<0) {
    decoder->cmdc=0;
    return -1;
  }
  return 0;
}
