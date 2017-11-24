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

  return 0;
}
