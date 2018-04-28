#include "akau_instrument_internal.h"
#include "akau_internal.h"
#include <stdlib.h>

/* New.
 */
 
static struct akau_instrument *akau_instrument_new_common(
  int attack_time,
  double attack_trim,
  int drawback_time,
  double drawback_trim,
  int decay_time
) {
  if (attack_time<0) return 0;
  if (attack_trim<0.0) return 0;
  if (drawback_time<0) return 0;
  if (drawback_trim<0.0) return 0;
  if (decay_time<0.0) return 0;
  if (attack_time>INT_MAX-decay_time) return 0;

  struct akau_instrument *instrument=calloc(1,sizeof(struct akau_instrument));
  if (!instrument) return 0;

  instrument->refc=1;
  instrument->attack_time=attack_time;
  instrument->drawback_time=drawback_time;
  instrument->decay_time=decay_time;
  instrument->attack_trim=attack_trim;
  instrument->drawback_trim=drawback_trim;

  return instrument;
}
 
struct akau_instrument *akau_instrument_new(
  struct akau_fpcm *fpcm,
  int attack_time,
  double attack_trim,
  int drawback_time,
  double drawback_trim,
  int decay_time
) {
  if (!fpcm) return 0;

  struct akau_instrument *instrument=akau_instrument_new_common(attack_time,attack_trim,drawback_time,drawback_trim,decay_time);
  if (!instrument) return 0;

  if (akau_fpcm_lock(fpcm)<0) {
    akau_instrument_del(instrument);
    return 0;
  }
  if (akau_fpcm_ref(fpcm)<0) {
    akau_fpcm_unlock(fpcm);
    akau_instrument_del(instrument);
    return 0;
  }
  instrument->fpcm=fpcm;

  if (
    !(instrument->ipcm=akau_ipcm_from_fpcm(fpcm,32767))||
    (akau_ipcm_lock(instrument->ipcm)<0)
  ) {
    akau_instrument_del(instrument);
    return 0;
  }

  return instrument;
}

struct akau_instrument *akau_instrument_new_deferred(
  int fpcmid,
  int attack_time,
  double attack_trim,
  int drawback_time,
  double drawback_trim,
  int decay_time
) {
  if (fpcmid<1) return 0;

  struct akau_instrument *instrument=akau_instrument_new_common(attack_time,attack_trim,drawback_time,drawback_trim,decay_time);
  if (!instrument) return 0;

  instrument->fpcmid=fpcmid;

  return instrument;
}

struct akau_instrument *akau_instrument_new_default(
  struct akau_fpcm *fpcm
) {
  return akau_instrument_new(fpcm,1000,0.7,1000,0.6,2000);
}

/* Delete.
 */

void akau_instrument_del(struct akau_instrument *instrument) {
  if (!instrument) return;
  if (instrument->refc-->1) return;

  akau_fpcm_unlock(instrument->fpcm);
  akau_fpcm_del(instrument->fpcm);
  akau_ipcm_unlock(instrument->ipcm);
  akau_ipcm_del(instrument->ipcm);

  free(instrument);
}

/* Retain.
 */
 
int akau_instrument_ref(struct akau_instrument *instrument) {
  if (!instrument) return -1;
  if (instrument->refc<1) return -1;
  if (instrument->refc==INT_MAX) return -1;
  instrument->refc++;
  return 0;
}

/* Trivial accessors.
 */

struct akau_fpcm *akau_instrument_get_fpcm(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->fpcm;
}

int akau_instrument_get_attack_time(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->attack_time;
}

int akau_instrument_get_drawback_time(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->drawback_time;
}

int akau_instrument_get_decay_time(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->decay_time;
}

double akau_instrument_get_attack_trim(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->attack_trim;
}

double akau_instrument_get_drawback_trim(const struct akau_instrument *instrument) {
  if (!instrument) return 0;
  return instrument->drawback_trim;
}

/* Decode integer.
 */

static int akau_instrument_decode_int(int *dst,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return -1;
  if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
  *dst=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    if (*dst>INT_MAX/10) return -1;
    *dst*=10;
    if (*dst>INT_MAX-digit) return -1;
    *dst+=digit;
  }
  return srcp;
}

/* Decode.
 */
 
struct akau_instrument *akau_instrument_decode(const char *src,int srcc) {
  int srcp=0,err;
  int attack_time,attack_trim,drawback_time,drawback_trim,decay_time;

  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp++]!='(')) return 0;
  
  if ((err=akau_instrument_decode_int(&attack_time,src+srcp,srcc-srcp))<0) return 0; srcp+=err;
  if ((err=akau_instrument_decode_int(&attack_trim,src+srcp,srcc-srcp))<0) return 0; srcp+=err;
  if ((err=akau_instrument_decode_int(&drawback_time,src+srcp,srcc-srcp))<0) return 0; srcp+=err;
  if ((err=akau_instrument_decode_int(&drawback_trim,src+srcp,srcc-srcp))<0) return 0; srcp+=err;
  if ((err=akau_instrument_decode_int(&decay_time,src+srcp,srcc-srcp))<0) return 0; srcp+=err;

  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp++]!=')')) return 0;

  int rate=akau_get_master_rate();
  if (attack_time>INT_MAX/rate) return 0;
  attack_time=(attack_time*rate)/1000;
  if (drawback_time>INT_MAX/rate) return 0;
  drawback_time=(drawback_time*rate)/1000;
  if (decay_time>INT_MAX/rate) return 0;
  decay_time=(decay_time*rate)/1000;

  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp<=srcc-8)&&!memcmp(src+srcp,"external",8)) {
    srcp+=8;
    int fpcmid;
    if ((err=akau_instrument_decode_int(&fpcmid,src+srcp,srcc-srcp))<0) return 0; srcp+=err;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if (srcp<srcc) return 0;
    return akau_instrument_new_deferred(fpcmid,attack_time,attack_trim/255.0,drawback_time,drawback_trim/255.0,decay_time);
  }

  struct akau_fpcm *fpcm=akau_decode_fpcm_harmonics(0,src+srcp,srcc-srcp,1);
  if (!fpcm) return 0;
  struct akau_instrument *instrument=akau_instrument_new(fpcm,attack_time,attack_trim/255.0,drawback_time,drawback_trim/255.0,decay_time);
  akau_fpcm_del(fpcm);
  return instrument;
}

/* Decode binary.
 */
 
struct akau_instrument *akau_instrument_decode_binary(const void *src,int srcc) {
  if (!src||(srcc<10)) return 0;
  const uint8_t *SRC=src;
  
  int attacktime=(SRC[0]<<8)|SRC[1];
  int drawbacktime=(SRC[2]<<8)|SRC[3];
  int decaytime=(SRC[4]<<8)|SRC[5];
  double attacktrim=SRC[6]/255.0;
  double drawbacktrim=SRC[7]/255.0;
  int coefc=SRC[9];
  if (srcc<10+coefc*2) return 0;

  // Encoded times are in milliseconds, but akau_instrument_new() expects them in frames.
  int rate=akau_get_master_rate();
  if (rate<1) return 0;
  attacktime=(attacktime*rate)/1000;
  drawbacktime=(drawbacktime*rate)/1000;
  decaytime=(decaytime*rate)/1000;

  // (coefc==0) is legal, but we must add one if that is the case. akau_generate_fpcm_harmonics() requires at least one.
  int dummy_coef=0;
  if (!coefc) {
    dummy_coef=1;
    coefc=1;
  }

  double *coefv=malloc(sizeof(double)*coefc);
  if (!coefv) return 0;
  if (dummy_coef) {
    coefv[0]=1.0;
  } else {
    int srcp=10;
    int i=0; for (;i<coefc;i++,srcp+=2) {
      coefv[i]=((SRC[srcp]<<8)|SRC[srcp+1])/65535.0;
    }
  }

  struct akau_fpcm *fpcm=akau_generate_fpcm_harmonics(0,coefv,coefc,1);
  free(coefv);
  if (!fpcm) return 0;

  struct akau_instrument *instrument=akau_instrument_new(fpcm,attacktime,attacktrim,drawbacktime,drawbacktrim,decaytime);
  akau_fpcm_del(fpcm);
  return instrument;
}

/* Represent decimal unsigned integer.
 */

static int akau_instrument_int_repr(char *dst,int dsta,int src) {
  if (src<0) src=0;
  int dstc=1,limit=10;
  while (src>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
  if (dstc>dsta) return dstc;
  int i=dstc; for (;i-->0;src/=10) dst[i]='0'+src%10;
  return dstc;
}

/* Text from binary.
 */
 
int akau_instrument_text_from_binary(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src||(srcc<10)) return -1;
  const uint8_t *SRC=src;
  
  int attacktime=(SRC[0]<<8)|SRC[1];
  int drawbacktime=(SRC[2]<<8)|SRC[3];
  int decaytime=(SRC[4]<<8)|SRC[5];
  int attacktrim=SRC[6];
  int drawbacktrim=SRC[7];
  int coefc=SRC[9];
  if (srcc<10+coefc*2) return 0;

  int dstc=0;
  if (dstc<dsta) dst[dstc]='('; dstc++;
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,attacktime);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,attacktrim);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,drawbacktime);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,drawbacktrim);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,decaytime);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  if (dstc<dsta) dst[dstc]=')'; dstc++;

  int srcp=10,i=0;
  for (;i<coefc;i++,srcp+=2) {
    int coef=(SRC[srcp]<<8)|SRC[srcp+1];
    coef=(coef*1000)/65535;
    if (coef>999) coef=999;
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    dstc+=akau_instrument_int_repr(dst+dstc,dsta-dstc,coef);
  }

  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Binary from text.
 */
 
int akau_instrument_binary_from_text(void *dst,int dsta,const char *src,int srcc) {
  int srcp=0,err;

  int attacktime,attacktrim,drawbacktime,drawbacktrim,decaytime;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp++]!='(')) return -1;
  
  if ((err=akau_instrument_decode_int(&attacktime,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_instrument_decode_int(&attacktrim,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_instrument_decode_int(&drawbacktime,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_instrument_decode_int(&drawbacktrim,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
  if ((err=akau_instrument_decode_int(&decaytime,src+srcp,srcc-srcp))<0) return -1; srcp+=err;

  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp++]!=')')) return -1;

  if (attacktime>0xffff) return -1;
  if (attacktrim>0xff) return -1;
  if (drawbacktime>0xffff) return -1;
  if (drawbacktrim>0xff) return -1;
  if (decaytime>0xffff) return -1;

  uint8_t *DST=dst;
  if (dsta>=10) {
    DST[0]=attacktime>>8;
    DST[1]=attacktime;
    DST[2]=drawbacktime>>8;
    DST[3]=drawbacktime;
    DST[4]=decaytime>>8;
    DST[5]=decaytime;
    DST[6]=attacktrim;
    DST[7]=drawbacktrim;
    DST[8]=0;
    DST[9]=0; // will fill in with coefc
  }
  int dstc=10;

  int coefc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (coefc>=255) return -1;
    int coef;
    if ((err=akau_instrument_decode_int(&coef,src+srcp,srcc-srcp))<0) return -1; srcp+=err;
    if (coef<0) coef=0;
    else if (coef>999) coef=999;
    coef=(coef*65535)/999;
    if (dstc<=dsta-2) {
      DST[dstc++]=coef>>8;
      DST[dstc++]=coef;
    } else dstc+=2;
    coefc++;
  }

  if (dsta>=10) DST[9]=coefc;

  return dstc;
}

/* Link.
 */
 
int akau_instrument_link(struct akau_instrument *instrument,struct akau_store *store) {
  if (!instrument) return -1;
  if (instrument->fpcm) return 0;
  struct akau_fpcm *fpcm=akau_store_get_fpcm(store,instrument->fpcmid);
  if (!fpcm) return akau_error("FPCM %d not found.",instrument->fpcmid);

  if (akau_fpcm_lock(fpcm)<0) return -1;
  if (akau_fpcm_ref(fpcm)<0) {
    akau_fpcm_unlock(fpcm);
    return -1;
  }
  instrument->fpcm=fpcm;

  if (instrument->ipcm) {
    akau_ipcm_unlock(instrument->ipcm);
    akau_ipcm_del(instrument->ipcm);
  }
  if (
    !(instrument->ipcm=akau_ipcm_from_fpcm(fpcm,32767))||
    (akau_ipcm_lock(instrument->ipcm)<0)
  ) {
    return -1;
  }

  return 0;
}
