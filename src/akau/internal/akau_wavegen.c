#include "akau_wavegen_internal.h"
#include <math.h>

/* Shared sine wave.
 */

static struct akau_fpcm *akau_wavegen_shared_sine=0;
 
struct akau_fpcm *akau_wavegen_get_shared_sine() {
  if (!akau_wavegen_shared_sine) {
    akau_wavegen_shared_sine=akau_generate_fpcm_sine(44100,0);
    akau_fpcm_lock(akau_wavegen_shared_sine);
  }
  return akau_wavegen_shared_sine;
}

void akau_wavegen_cleanup_shared_sine() {
  akau_fpcm_del(akau_wavegen_shared_sine);
  akau_wavegen_shared_sine=0;
}

/* Single wave generators, helper macros.
 */

#define GENFPCM \
  struct akau_fpcm *fpcm=akau_fpcm_new(samplec); \
  if (!fpcm) return 0; \
  if (akau_fpcm_set_loop(fpcm,0,samplec)<0) { \
    akau_fpcm_del(fpcm); \
    return 0; \
  } \
  double *samplev=akau_fpcm_get_sample_buffer(fpcm); \
  if (!samplev) return 0;

#define GENIPCM \
  struct akau_ipcm *ipcm=akau_ipcm_new(samplec); \
  if (!ipcm) return 0; \
  if (akau_ipcm_set_loop(ipcm,0,samplec)<0) { \
    akau_ipcm_del(ipcm); \
    return 0; \
  } \
  int16_t *samplev=akau_ipcm_get_sample_buffer(ipcm); \
  if (!samplev) return 0;

/* Generate sine wave.
 */
 
struct akau_fpcm *akau_generate_fpcm_sine(int samplec,int8_t balance) {
  GENFPCM
  int crossp;
  if (balance) {
    crossp=samplec>>1;
    crossp+=(balance*crossp)>>7;
    if (crossp>=samplec) crossp=samplec-1;
  }
  int i=0; for (;i<samplec;i++,samplev++) {
    double adjp;
    if (balance) {
      if (i<crossp) {
        adjp=(i*M_PI)/crossp;
      } else {
        adjp=M_PI+((i-crossp)*M_PI)/(samplec-crossp);
      }
    } else {
      adjp=(i*M_PI*2.0)/samplec;
    }
    *samplev=sin(adjp);
  }
  return fpcm;
}

struct akau_ipcm *akau_generate_ipcm_sine(int samplec,int8_t balance) {
  GENIPCM
  int crossp;
  if (balance) {
    crossp=samplec>>1;
    crossp+=(balance*crossp)>>7;
    if (crossp>=samplec) crossp=samplec-1;
  }
  int i=0; for (;i<samplec;i++,samplev++) {
    double adjp;
    if (balance) {
      if (i<crossp) {
        adjp=(i*M_PI)/crossp;
      } else {
        adjp=M_PI+((i-crossp)*M_PI)/(samplec-crossp);
      }
    } else {
      adjp=(i*M_PI*2.0)/samplec;
    }
    double sample=sin(adjp)*32767.0;
    if (sample<=-32768.0) *samplev=-32768;
    else if (sample>=32767.0) *samplev=32767;
    else *samplev=(int16_t)sample;
  }
  return ipcm;
}

/* Generate square wave.
 */

struct akau_fpcm *akau_generate_fpcm_square(int samplec,int8_t balance) {
  GENFPCM
  int crossp=samplec>>1;
  if (balance) crossp+=(balance*crossp)>>7;
  int i=0;
  for (;i<crossp;i++,samplev++) *samplev=1.0;
  for (;i<crossp;i++,samplev++) *samplev=-1.0;
  return fpcm;
}

struct akau_ipcm *akau_generate_ipcm_square(int samplec,int8_t balance) {
  GENIPCM
  int crossp=samplec>>1;
  if (balance) crossp+=(balance*crossp)>>7;
  int i=0; 
  for (;i<crossp;i++,samplev++) *samplev=-32768;
  for (;i<samplec;i++,samplev++) *samplev=32767;
  return ipcm;
}

/* Generate sawtooth wave.
 */
 
struct akau_fpcm *akau_generate_fpcm_saw(int samplec) {
  GENFPCM
  int i=0; for (;i<samplec;i++,samplev++) {
    *samplev=1.0-(i*2.0)/samplec;
  }
  return fpcm;
}

struct akau_ipcm *akau_generate_ipcm_saw(int samplec) {
  GENIPCM
  int i=0; for (;i<samplec;i++,samplev++) {
    *samplev=32767-(i*65535)/samplec;
  }
  return ipcm;
}

/* Generate white noise.
 */
 
struct akau_fpcm *akau_generate_fpcm_whitenoise(int samplec) {
  GENFPCM
  int i=samplec; for (;i-->0;samplev++) {
    *samplev=(rand()&0xffff)/32768.0-1.0;
  }
  return fpcm;
}

struct akau_ipcm *akau_generate_ipcm_whitenoise(int samplec) {
  GENIPCM
  int i=samplec; for (;i-->0;samplev++) {
    *samplev=rand();
  }
  return ipcm;
}

#undef GENFPCM
#undef GENIPCM

/* Generate harmonics.
 */
 
struct akau_fpcm *akau_generate_fpcm_harmonics(
  const struct akau_fpcm *reference,
  const double *coefv,int coefc,
  int normalize
) {
  if (!reference&&!(reference=akau_wavegen_get_shared_sine())) return 0;
  if (!coefv||(coefc<1)) return 0;

  int samplec=akau_fpcm_get_sample_count(reference);
  if (samplec<1) return 0;
  const double *srcv=akau_fpcm_get_sample_buffer((struct akau_fpcm*)reference); // Cast away const, transiently.
  if (!srcv) return 0;
  if (coefc>=samplec) coefc=samplec-1;

  struct akau_fpcm *dst=akau_fpcm_new(samplec);
  if (!dst) return 0;
  if (akau_fpcm_set_loop(dst,0,samplec)<0) {
    akau_fpcm_del(dst);
    return 0;
  }
  double *dstv=akau_fpcm_get_sample_buffer(dst);
  if (!dstv) {
    akau_fpcm_del(dst);
    return 0;
  }

  int coefp=0; for (;coefp<coefc;coefp++) {
    int harmonic_number=coefp+1;
    double coef=coefv[coefp];
    if (normalize) coef/=harmonic_number;
    int dstp=0,srcp=0;
    for (;dstp<samplec;dstp++,srcp+=harmonic_number) {
      if (srcp>=samplec) srcp-=samplec;
      dstv[dstp]+=srcv[srcp]*coef;
    }
  }

  return dst;
}

/* Generate harmonics from text.
 */
 
struct akau_fpcm *akau_decode_fpcm_harmonics(
  const struct akau_fpcm *reference,
  const char *src,int srcc,
  int normalize
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  double coefv[16];
  int coefc=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    int coef=0;
    if ((src[srcp]<'0')||(src[srcp]>'9')) return 0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      coef*=10;
      coef+=digit;
      if (coef>1000) return 0;
    }
    if (coefc<sizeof(coefv)/sizeof(double)) {
      coefv[coefc++]=coef/1000.0;
    }
  }

  if (!coefc) coefv[coefc++]=1.0;
  return akau_generate_fpcm_harmonics(reference,coefv,coefc,normalize);
}

/* Calculate rate for pitch.
 */

static const double akau_musical_intervals[12]={
  1.000000,
  1.059463,
  1.122462,
  1.189207,
  1.259921,
  1.334840,
  1.414214,
  1.498307,
  1.587401,
  1.681793,
  1.781797,
  1.887749,
};
 
double akau_rate_from_pitch(uint8_t pitch) {
  double rate=AKAU_A0_FREQ;
  while (pitch>=12) {
    rate*=2.0;
    pitch-=12;
  }
  return rate*akau_musical_intervals[pitch];
}

/* Resample.
 */

struct akau_ipcm *akau_resample_ipcm(struct akau_ipcm *src,int from_hz,int to_hz) {
  if (!src) return 0;
  if (from_hz==to_hz) return akau_ipcm_copy(src);
  if ((from_hz<1000)||(from_hz>1000000)) return 0;
  if ((to_hz<1000)||(to_hz>1000000)) return 0;

  if (src->c>INT_MAX/to_hz) return 0;
  int dstsamplec=(src->c*to_hz)/from_hz;
  struct akau_ipcm *dst=akau_ipcm_new(dstsamplec);
  if (!dst) return 0;

  if (to_hz>from_hz) {
    /* Upsampling: Interpolate between input samples. This is expensive. */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      double srcpf,whole,fract;
      srcpf=((double)dstp*src->c)/dstsamplec;
      fract=modf(srcpf,&whole);
      int srcp=(int)srcpf;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      int16_t samplea,sampleb;
      samplea=src->v[srcp];
      if (srcp<src->c-1) sampleb=src->v[srcp+1]; else sampleb=samplea;
      if (samplea!=sampleb) {
        double sample=(samplea*(1.0-fract))+(sampleb*fract);
        if (sample>=32767.0) samplea=32767;
        else if (sample<=-32768.0) samplea=-32768;
        else samplea=(int16_t)sample;
      }
      dst->v[dstp]=samplea;
    }
  } else if (dstsamplec>INT_MAX/src->c) {
    /* Downsampling, but simple scaling arithmetic would overflow. :( */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      int64_t srcp=dstp;
      srcp*=src->c;
      srcp/=dstsamplec;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      dst->v[dstp]=src->v[srcp];
    }
  } else {
    /* Downsampling: Take the nearest input sample. This is a lot cheaper. */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      int srcp=(dstp*src->c)/dstsamplec;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      dst->v[dstp]=src->v[srcp];
    }
  }
  
  return dst;
}

struct akau_fpcm *akau_resample_fpcm(struct akau_fpcm *src,int from_hz,int to_hz) {
  if (!src) return 0;
  if (from_hz==to_hz) return akau_fpcm_copy(src);
  if ((from_hz<1000)||(from_hz>1000000)) return 0;
  if ((to_hz<1000)||(to_hz>1000000)) return 0;

  if (src->c>INT_MAX/to_hz) return 0;
  int dstsamplec=(src->c*to_hz)/from_hz;
  struct akau_fpcm *dst=akau_fpcm_new(dstsamplec);
  if (!dst) return 0;

  if (to_hz>from_hz) {
    /* Upsampling: Interpolate between input samples. This is expensive. */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      double srcpf,whole,fract;
      srcpf=((double)dstp*src->c)/dstsamplec;
      fract=modf(srcpf,&whole);
      int srcp=(int)srcpf;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      double samplea,sampleb;
      samplea=src->v[srcp];
      if (srcp<src->c-1) sampleb=src->v[srcp+1]; else sampleb=samplea;
      dst->v[dstp]=(samplea*(1.0-fract))+(sampleb*fract);
    }
  } else if (dstsamplec>INT_MAX/src->c) {
    /* Downsampling, but simple scaling arithmetic would overflow. :( */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      int64_t srcp=dstp;
      srcp*=src->c;
      srcp/=dstsamplec;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      dst->v[dstp]=src->v[srcp];
    }
  } else {
    /* Downsampling: Take the nearest input sample. This is a lot cheaper. */
    int dstp=0; for (;dstp<dstsamplec;dstp++) {
      int srcp=(dstp*src->c)/dstsamplec;
      if (srcp<0) srcp=0; else if (srcp>=src->c) srcp=src->c-1;
      dst->v[dstp]=src->v[srcp];
    }
  }
  
  return dst;
}

/* Decode from long form text.
 */
 
struct akau_ipcm *akau_wavegen_decode(const char *src,int srcc) {
  struct akau_wavegen_decoder *decoder=akau_wavegen_decoder_new();
  if (!decoder) return 0;
  if (akau_wavegen_decoder_decode(decoder,src,srcc)<0) {
    akau_wavegen_decoder_del(decoder);
    return 0;
  }
  struct akau_ipcm *ipcm=akau_wavegen_decoder_print(decoder);
  akau_wavegen_decoder_del(decoder);
  return ipcm;
}
