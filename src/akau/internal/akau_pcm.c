#include "akau_pcm_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* New.
 */

struct akau_ipcm *akau_ipcm_new(int samplec) {

  if (samplec<1) return 0;
  int headersize=sizeof(struct akau_ipcm);
  if (samplec>INT_MAX/sizeof(int16_t)) return 0;
  int datasize=samplec*sizeof(int16_t);
  if (headersize>INT_MAX-datasize) return 0;

  struct akau_ipcm *ipcm=calloc(1,headersize+datasize);
  if (!ipcm) return 0;

  ipcm->refc=1;
  ipcm->c=samplec;
  ipcm->loopap=-1;
  ipcm->loopzp=-1;

  return ipcm;
}

struct akau_fpcm *akau_fpcm_new(int samplec) {

  if (samplec<1) return 0;
  int headersize=sizeof(struct akau_fpcm);
  if (samplec>INT_MAX/sizeof(double)) return 0;
  int datasize=samplec*sizeof(double);
  if (headersize>INT_MAX-datasize) return 0;

  struct akau_fpcm *fpcm=calloc(1,headersize+datasize);
  if (!fpcm) return 0;

  fpcm->refc=1;
  fpcm->c=samplec;
  fpcm->loopap=-1;
  fpcm->loopzp=-1;

  return fpcm;
}

/* Delete.
 */
  
void akau_ipcm_del(struct akau_ipcm *ipcm) {
  if (!ipcm) return;
  if (ipcm->refc-->1) return;
  free(ipcm);
}

/* Retain.
 */
 
int akau_ipcm_ref(struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  if (ipcm->refc<1) return -1;
  if (ipcm->refc==INT_MAX) return -1;
  ipcm->refc++;
  return 0;
}

/* Lock.
 */

int akau_ipcm_lock(struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  if (ipcm->lock<0) return -1;
  if (ipcm->lock==INT_MAX) return -1;
  ipcm->lock++;
  return 0;
}

/* Unlock.
 */
 
int akau_ipcm_unlock(struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  if (ipcm->lock<1) return -1;
  ipcm->lock--;
  return 0;
}

/* Trivial accessors.
 */

int akau_ipcm_get_sample_count(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  return ipcm->c;
}

int akau_ipcm_has_loop(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  return (ipcm->loopap>=0)?1:0;
}

int akau_ipcm_get_loop_start(const struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  return ipcm->loopap;
}

int akau_ipcm_get_loop_end(const struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  return ipcm->loopzp;
}

int akau_ipcm_is_locked(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  return ipcm->lock?1:0;
}

int16_t *akau_ipcm_get_sample_buffer(struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  return ipcm->v;
}

double *akau_fpcm_get_sample_buffer(struct akau_fpcm *fpcm) {
  if (!fpcm) return 0;
  return fpcm->v;
}

/* Get sample.
 */

int16_t akau_ipcm_get_sample(const struct akau_ipcm *ipcm,int p) {
  if (!ipcm) return 0;
  if ((p<0)||(p>=ipcm->c)) return 0;
  return ipcm->v[p];
}

double akau_fpcm_get_sample(const struct akau_fpcm *fpcm,int p) {
  if (!fpcm) return 0.0;
  if ((p<0)||(p>=fpcm->c)) return 0.0;
  return fpcm->v[p];
}

/* Advance position.
 */

int akau_ipcm_advance(struct akau_ipcm *ipcm,int p,int addp,int loop) {
  if (!ipcm) return -1;
  if (p<0) return -1;
  if (p>=ipcm->c) return -1;
  if (addp<1) return -1;
  p+=addp;
  if (loop&&(ipcm->loopap>=0)) {
    if (p>=ipcm->loopzp) p-=(ipcm->loopzp-ipcm->loopap);
  }
  return p;
}

/* Set sample.
 */
 
int akau_ipcm_set_sample(struct akau_ipcm *ipcm,int p,int16_t sample) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  if ((p<0)||(p>=ipcm->c)) return -1;
  ipcm->v[p]=sample;
  return 0;
}

int akau_fpcm_set_sample(struct akau_fpcm *fpcm,int p,double sample) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  if ((p<0)||(p>=fpcm->c)) return -1;
  fpcm->v[p]=sample;
  return 0;
}

/* Set loop.
 */
 
int akau_ipcm_set_loop(struct akau_ipcm *ipcm,int start,int end) {
  if (!ipcm) return -1;
  if ((start<0)||(start>=ipcm->c)) return -1;
  if ((end<=start)||(end>ipcm->c)) return -1;
  ipcm->loopap=start;
  ipcm->loopzp=end;
  return 0;
}

int akau_ipcm_unset_loop(struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  ipcm->loopap=-1;
  ipcm->loopzp=-1;
  return 0;
}

/* Clear.
 */

int akau_ipcm_clear(struct akau_ipcm *ipcm) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  memset(ipcm->v,0,sizeof(int16_t)*ipcm->c);
  return 0;
}

int akau_fpcm_clear(struct akau_fpcm *fpcm) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  memset(fpcm->v,0,sizeof(double)*fpcm->c);
  return 0;
}

/* Amplify.
 */
 
int akau_ipcm_amplify(struct akau_ipcm *ipcm,double multiplier) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  int16_t *v=ipcm->v;
  int i=ipcm->c; for (;i-->0;v++) {
    double sample=((double)*v)*multiplier;
    if (sample<=-32768.0) {
      *v=-32768;
    } else if (sample>=32767.0) {
      *v=32767;
    } else {
      *v=(int16_t)sample;
    }
  }
  return 0;
}

int akau_fpcm_amplify(struct akau_fpcm *fpcm,double multiplier) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  double *v=fpcm->v;
  int i=fpcm->c; for (;i-->0;v++) {
    *v*=multiplier;
  }
  return 0;
}

/* Clip.
 */
 
int akau_ipcm_clip(struct akau_ipcm *ipcm,int16_t threshold) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  int16_t lo,hi;
  if (threshold<0) {
    if (threshold==-32768) return 0;
    lo=threshold;
    hi=-threshold;
  } else {
    lo=-threshold;
    hi=threshold;
  }
  int modc=0;
  int16_t *v=ipcm->v;
  int i=ipcm->c; for (;i-->0;v++) {
    if (*v<lo) {
      *v=lo;
      modc++;
    } else if (*v>hi) {
      *v=hi;
      modc++;
    }
  }
  return modc;
}

int akau_fpcm_clip(struct akau_fpcm *fpcm,double threshold) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  double lo,hi;
  if (threshold<0.0) {
    lo=threshold;
    hi=-threshold;
  } else {
    lo=-threshold;
    hi=threshold;
  }
  int modc=0;
  double *v=fpcm->v;
  int i=fpcm->c; for (;i-->0;v++) {
    if (*v<lo) {
      *v=lo;
      modc++;
    } else if (*v>hi) {
      *v=hi;
      modc++;
    }
  }
  return modc;
}

/* Fade in.
 */
 
int akau_ipcm_fade_in(struct akau_ipcm *ipcm,int samplec) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  if (samplec<0) return -1;
  if (samplec>ipcm->c) return -1;
  int16_t *v=ipcm->v;
  int i=0; for (;i<samplec;i++,v++) {
    *v=(*v*i)/samplec;
  }
  return 0;
}
 
int akau_fpcm_fade_in(struct akau_fpcm *fpcm,int samplec) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  if (samplec<0) return -1;
  if (samplec>fpcm->c) return -1;
  double *v=fpcm->v;
  int i=0; for (;i<samplec;i++,v++) {
    *v=(*v*i)/samplec;
  }
  return 0;
}

/* Fade out.
 */
 
int akau_ipcm_fade_out(struct akau_ipcm *ipcm,int samplec) {
  if (!ipcm) return -1;
  if (ipcm->lock) return -1;
  if (samplec<0) return -1;
  if (samplec>ipcm->c) return -1;
  int16_t *v=ipcm->v+ipcm->c-samplec;
  int i=samplec; for (;i-->0;v++) {
    *v=(*v*i)/samplec;
  }
  return 0;
}
 
int akau_fpcm_fade_out(struct akau_fpcm *fpcm,int samplec) {
  if (!fpcm) return -1;
  if (fpcm->lock) return -1;
  if (samplec<0) return -1;
  if (samplec>fpcm->c) return -1;
  double *v=fpcm->v+fpcm->c-samplec;
  int i=samplec; for (;i-->0;v++) {
    *v=(*v*i)/samplec;
  }
  return 0;
}

/* Add noise.
 */
 
int akau_ipcm_noisify(struct akau_ipcm *ipcm,double amount,int relative) {
  if (!ipcm) return -1;
  if ((amount<0.0)||(amount>1.0)) return -1;
  uint8_t iamt;
  if (amount>=1.0) iamt=0xff;
  else if (amount<=0.0) iamt=0;
  else iamt=(int)(amount*255.0);
  int16_t *v=ipcm->v;
  int i=ipcm->c; for (;i-->0;v++) {
    int amp;
    if (relative) {
      int limit=(*v*iamt)>>8;
      if (!limit) amp=*v;
      else amp=*v+rand()%limit;
    } else {
      int16_t noise=rand();
      noise=(noise*iamt)>>8;
      amp=*v+noise;
    }
    if (amp>=32767) *v=32767;
    else if (amp<=-32768) *v=-32768;
    else *v=amp;
  }
  return 0;
}

int akau_fpcm_noisify(struct akau_fpcm *fpcm,double amount,int relative) {
  if (!fpcm) return -1;
  if ((amount<0.0)||(amount>1.0)) return -1;
  double *v=fpcm->v;
  int i=fpcm->c; for (;i-->0;v++) {
    double r=((rand()&0xffff)/65535.0)*amount;
    if (relative) {
      *v*=r;
    } else {
      *v+=r;
    }
  }
  return 0;
}

/* RMS.
 */
 
int16_t akau_ipcm_calculate_rms(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  // This could involve very large numbers, so we calculate floating-point and convert back to int later.
  double sum=0.0;
  const int16_t *src=ipcm->v;
  int i=ipcm->c; for (;i-->0;src++) {
    sum+=(*src)*(*src);
  }
  double frms=sqrt(sum/ipcm->c);
  if (frms>=32767.0) return 32767;
  return (int16_t)frms;
}

double akau_fpcm_calculate_rms(const struct akau_fpcm *fpcm) {
  if (!fpcm) return 0.0;
  double sum=0.0;
  const double *src=fpcm->v;
  int i=fpcm->c; for (;i-->0;src++) {
    sum+=(*src)*(*src);
  }
  return sqrt(sum/fpcm->c);
}

/* Peak.
 */
 
int16_t akau_ipcm_calculate_peak(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  int16_t peak=0;
  const int16_t *src=ipcm->v;
  int i=ipcm->c; for (;i-->0;src++) {
    int16_t sample=*src;
    if (sample==-32768) return 32767;
    if (sample<0) sample=-sample;
    if (sample>peak) peak=sample;
  }
  return peak;
}

double akau_fpcm_calculate_peak(const struct akau_fpcm *fpcm) {
  if (!fpcm) return 0.0;
  double peak=0.0;
  const double *src=fpcm->v;
  int i=fpcm->c; for (;i-->0;src++) {
    double sample=*src;
    if (sample<0.0) sample=-sample;
    if (sample>peak) peak=sample;
  }
  return peak;
}

/* ipcm from fpcm.
 */
 
struct akau_ipcm *akau_ipcm_from_fpcm(const struct akau_fpcm *fpcm,int16_t normal) {
  if (!fpcm) return 0;
  if (normal<1) return 0;

  double srcrms=akau_fpcm_calculate_rms(fpcm);
  double multiplier=0.0;
  if (srcrms>0.0) multiplier=(double)normal/srcrms;

  struct akau_ipcm *ipcm=akau_ipcm_new(fpcm->c);
  if (!ipcm) return 0;
  ipcm->loopap=fpcm->loopap;
  ipcm->loopzp=fpcm->loopzp;

  const double *src=fpcm->v;
  int16_t *dst=ipcm->v;
  int c=fpcm->c; for (;c-->0;src++,dst++) {
    double sample=(double)*src*multiplier;
    if (sample>=32767.0) *dst=32767;
    else if (sample<=-32768.0) *dst=-32768;
    else *dst=(int)sample;
  }

  return ipcm;
}

/* fpcm from ipcm.
 */

struct akau_fpcm *akau_fpcm_from_ipcm(const struct akau_ipcm *ipcm,int16_t normal) {
  if (!ipcm) return 0;
  if (normal<1) return 0;
  double fnormal=(double)normal;

  struct akau_fpcm *fpcm=akau_fpcm_new(ipcm->c);
  if (!fpcm) return 0;
  fpcm->loopap=ipcm->loopap;
  fpcm->loopzp=ipcm->loopzp;

  const int16_t *src=ipcm->v;
  double *dst=fpcm->v;
  int i=ipcm->c; for (;i-->0;src++,dst++) {
    *dst=(double)*src/fnormal;
  }

  return fpcm;
}

/* Copy.
 */
 
struct akau_ipcm *akau_ipcm_copy(const struct akau_ipcm *ipcm) {
  if (!ipcm) return 0;
  struct akau_ipcm *dst=akau_ipcm_new(ipcm->c);
  if (!dst) return 0;
  dst->loopap=ipcm->loopap;
  dst->loopzp=ipcm->loopzp;
  memcpy(dst->v,ipcm->v,sizeof(int16_t)*ipcm->c);
  return dst;
}

struct akau_fpcm *akau_fpcm_copy(const struct akau_fpcm *fpcm) {
  if (!fpcm) return 0;
  struct akau_fpcm *dst=akau_fpcm_new(fpcm->c);
  if (!dst) return 0;
  dst->loopap=fpcm->loopap;
  dst->loopzp=fpcm->loopzp;
  memcpy(dst->v,fpcm->v,sizeof(double)*fpcm->c);
  return dst;
}
