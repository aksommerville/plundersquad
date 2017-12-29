#include "akau_pcm_internal.h"
#include "akau_internal.h"
#include "../akau_wavegen.h"
#include <string.h>
#include <limits.h>

#define B(v,p) (((uint8_t*)(v))[p])
#define RL16(v) (B(v,0)|(B(v,1)<<8))
#define RL32(v) (B(v,0)|(B(v,1)<<8)|(B(v,2)<<16)|(B(v,3)<<24))
#define RB16(v) ((B(v,0)<<8)|B(v,1))
#define RB32(v) ((B(v,0)<<24)|(B(v,1)<<16)|(B(v,2)<<8)|B(v,3))

/* Format magic.
 */

#define AKAU_PCM_FORMAT_NONE      0 /* Unrecognized. */
#define AKAU_PCM_FORMAT_WAV       1 /* We accept a subset of Microsoft WAV. */
#define AKAU_PCM_FORMAT_AKAUPCM   2 /* Our own raw PCM format. */
#define AKAU_PCM_FORMAT_WAVEGEN   3 /* One-way synthesizer. */

static int akau_pcm_detect_format(const char *src,int srcc) {
  if (!src) return AKAU_PCM_FORMAT_NONE;

  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp(src+8,"WAVE",4)) {
    return AKAU_PCM_FORMAT_WAV;
  }

  if ((srcc>=8)&&!memcmp(src,"\0AKAUPCM",8)) {
    return AKAU_PCM_FORMAT_AKAUPCM;
  }

  /* Not a signature, but pretty reliable... */
  if ((srcc>=7)&&!memcmp(src,"channel",7)) {
    return AKAU_PCM_FORMAT_WAVEGEN;
  }

  return AKAU_PCM_FORMAT_NONE;
}

/* Read a WAV file, parse its "fmt " chunk, and add up size of "data" chunks.
 * Input should point to the first chunk ID, ie 12 bytes into the file.
 * After reading this, you still will need to parse chunks to locate the data.
 */

struct akau_wav_header {
  int chanc; // can use as indicator for "fmt " present.
  int rate; // hz
  int samplesize; // bytes (file stores bits but we reject anything not a multiple of 8)
  int datac; // bytes; total size of all data chunks
};

static int akau_wav_read_header_fmt(struct akau_wav_header *header,const uint8_t *src,int srcc) {

  if (header->chanc) {
    akau_error("Multiple 'fmt ' chunks in WAV.");
    return -1;
  }
  if (srcc<16) {
    akau_error("WAV 'fmt ' chunk too small (%d<16)",srcc);
    return -1;
  }

  int format=RL16(src);
  if (format!=1) {
    akau_error("Unknown data format %d for WAV.",format);
    return -1;
  }
  header->chanc=RL16(src+2);
  if (header->chanc<1) {
    akau_error("Illegal channel count %d for WAV.",header->chanc);
    return -1;
  }
  header->rate=RL32(src+4);
  if ((header->rate<1000)||(header->rate>1000000)) {
    akau_error("Rejecting unreasonable sample rate %d Hz.",header->rate);
    return -1;
  }
  int byterate=RL32(src+8);
  int blockalign=RL16(src+12);
  header->samplesize=RL16(src+14);
  if (!header->samplesize||(header->samplesize&7)) {
    akau_error("Rejecting unreasonable sample size %d bits.",header->samplesize);
    return -1;
  }
  header->samplesize>>=3;

  if (blockalign!=header->chanc*header->samplesize) {
    akau_error("Rejecting WAV due to BlockAlign=%d (NumChannels=%d, BitsPerSample=%d).",blockalign,header->chanc,header->samplesize<<3);
    return -1;
  }
  if (byterate!=blockalign*header->rate) {
    akau_error("Rejecting WAV due to ByteRate=%d (frame size %d, sample rate %d).",byterate,blockalign,header->rate);
    return -1;
  }

  return 0;
}

static int akau_wav_read_header_data(struct akau_wav_header *header,const uint8_t *src,int srcc) {
  if (header->datac>INT_MAX-srcc) return -1;
  header->datac+=srcc;
  return 0;
}

static int akau_wav_read_header(struct akau_wav_header *header,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<=srcc-8) {
    const char *chunkid=(char*)src+srcp;
    int chunksize=RL32(src+srcp+4);
    if (chunksize<0) {
      akau_error("Illegal chunk size %d in WAV.",chunksize);
      return -1;
    }
    srcp+=8;
    if (srcp>srcc-chunksize) {
      akau_error("WAV chunk exceeds input size.");
      return -1;
    }
    if (!memcmp(chunkid,"fmt ",4)) {
      if (akau_wav_read_header_fmt(header,src+srcp,chunksize)<0) return -1;
    } else if (!memcmp(chunkid,"data",4)) {
      if (akau_wav_read_header_data(header,src+srcp,chunksize)<0) return -1;
    }
    srcp+=chunksize;
  }
  if (!header->chanc) {
    akau_error("WAV missing 'fmt ' chunk.");
    return -1;
  }
  if (header->datac<1) {
    akau_error("No data in WAV.");
    return -1;
  }
  return 0;
}

/* Decode 'data' chunk from WAV into ipcm.
 */

static int akau_ipcm_decode_wav_data(struct akau_ipcm *ipcm,int dstp,const struct akau_wav_header *header,const uint8_t *src,int srcc) {
  int framesize=header->samplesize*header->chanc;
  int srcsamplec=srcc/framesize;
  if (dstp>ipcm->c-srcsamplec) return -1; // Too much data; we must have miscalculated somewhere.

  /* It is very likely that the input format is identical to the output. Worth checking for. */
  #if BYTE_ORDER==LITTLE_ENDIAN
    if ((header->samplesize==2)&&(header->chanc==1)) {
      memcpy(ipcm->v+dstp,src,srcc);
      return srcsamplec;
    }
  #endif

  /* I'm told that 8-bit WAV is unsigned, ie 0..255. That takes special consideration.
   * I assume that any size larger than 16 is signed twos-complement, just like 16.
   */
  if (header->samplesize==1) {
    int16_t *dst=ipcm->v+dstp;
    int i=srcsamplec; for (;i-->0;src+=header->chanc,dst++) {
      uint8_t sample=src[0]-128;
      *dst=(sample<<8)|sample;
    }
    return srcsamplec;
  }

  /* Input is always little-endian.
   * Skip a few bytes so we are pointing initially at the most significant 2 bytes of the first channel of the first frame.
   */
  src+=header->samplesize-2;

  /* Read general PCM. */
  int16_t *dst=ipcm->v+dstp;
  int i=srcsamplec; for (;i-->0;src+=framesize,dst++) {
    *dst=(src[0]|(src[1]<<8));
  }

  return srcsamplec;
}

/* Decode Microsoft WAV.
 */

static struct akau_ipcm *akau_ipcm_decode_wav(const uint8_t *src,int srcc) {

  /* RIFF header. */
  if ((srcc<12)||memcmp(src,"RIFF",4)||memcmp(src+8,"WAVE",4)) {
    akau_error("Malformed WAV header, signature mismatch.");
    return 0;
  }

  /* WAV files typically have a "fmt " chunk followed by a "data" chunk.
   * I don't know if that is convention or requirement.
   * We'll play it nice and loose, allowing multiple "data", "fmt " wherever, and ignore unknown chunks.
   */
  struct akau_wav_header header={0};
  if (akau_wav_read_header(&header,src+12,srcc-12)<0) {
    return 0;
  }

  /* Create ipcm object. */
  int samplec=header.datac/header.samplesize;
  struct akau_ipcm *ipcm=akau_ipcm_new(samplec);
  if (!ipcm) return 0;

  /* Locate data chunks and read them into the ipcm. */
  int srcp=12;
  int dstp=0;
  while (dstp<samplec) {
    if (srcp>srcc-8) {
      akau_ipcm_del(ipcm);
      return 0;
    }
    const char *chunkid=(char*)src+srcp;
    int chunksize=RL32(src+srcp+4);
    srcp+=8;
    if (srcp>srcc-chunksize) {
      akau_ipcm_del(ipcm);
      return 0;
    }

    if (!memcmp(chunkid,"data",4)) {
      int dstaddc=akau_ipcm_decode_wav_data(ipcm,dstp,&header,src+srcp,chunksize);
      if (dstaddc<0) {
        akau_ipcm_del(ipcm);
        return 0;
      }
      dstp+=dstaddc;
    }
    
    srcp+=chunksize;
  }

  int master_rate=akau_get_master_rate();
  if (header.rate!=master_rate) {
    struct akau_ipcm *n=akau_resample_ipcm(ipcm,header.rate,master_rate);
    akau_ipcm_del(ipcm);
    ipcm=n;
  }

  return ipcm;
}

/* Decode AKAU PCM.
 */

static struct akau_ipcm *akau_ipcm_decode_akaupcm(const uint8_t *src,int srcc) {

  /* Assert header length and signature. */
  if (srcc<24) {
    akau_error("File too small to be AKAU PCM (%d).",srcc);
    return 0;
  }
  if (memcmp(src,"\0AKAUPCM",8)) {
    akau_error("AKAUPCM signature mismatch.");
    return 0;
  }

  /* Decode header. */
  int rate=RB32(src+8);
  int loopap=RB32(src+12);
  int loopzp=RB32(src+16);
  int samplefmt=RB32(src+20);

  /* Validate header. */
  if ((rate<1000)||(rate>1000000)) {
    akau_error("Improbable sample rate %d for AKAUPCM, rejecting.",rate);
    return 0;
  }
  if (loopap<0) {
    if ((loopap!=-1)||(loopzp!=-1)) {
      akau_error("Invalid loop endpoints for AKAUPCM (%d,%d).",loopap,loopzp);
      return 0;
    }
  } else if (loopap>=loopzp) {
    akau_error("Invalid loop endpoints for AKAUPCM (%d,%d).",loopap,loopzp);
    return 0;
  }

  /* Confirm sample format and byte order. */
  int swap_bytes;
  switch (samplefmt) {
    #if BYTE_ORDER==LITTLE_ENDIAN
      case 1: swap_bytes=0; break;
      case 2: swap_bytes=1; break;
    #else
      case 1: swap_bytes=1; break;
      case 2: swap_bytes=0; break;
    #endif
    default: {
        akau_error("Unknown sample format %d in AKAUPCM.",samplefmt);
        return 0;
      }
  }

  /* Measure raw input. */
  int input_size=srcc-24; // 24 byte header.
  if ((input_size&1)||(input_size<1)) {
    akau_error("File size for AKAUPCM does not compute.");
    return 0;
  }
  int samplec=input_size>>1;
  if (loopzp>samplec) {
    akau_error("Illegal loop endpoints (%d,%d) for %d samples.",loopap,loopzp,samplec);
    return 0;
  }

  /* Create the object. */
  struct akau_ipcm *ipcm=akau_ipcm_new(samplec);
  if (!ipcm) return 0;
  ipcm->loopap=loopap;
  ipcm->loopzp=loopzp;

  /* Copy samples. */
  if (swap_bytes) {
    const uint8_t *samplesrc=src+24;
    uint8_t *sampledst=(uint8_t*)(ipcm->v);
    int i=samplec; for (;i-->0;samplesrc+=2,sampledst+=2) {
      sampledst[0]=samplesrc[1];
      sampledst[1]=samplesrc[0];
    }
  } else {
    memcpy(ipcm->v,src+24,input_size);
  }

  int master_rate=akau_get_master_rate();
  if (rate!=master_rate) {
    struct akau_ipcm *n=akau_resample_ipcm(ipcm,rate,master_rate);
    akau_ipcm_del(ipcm);
    ipcm=n;
  }
  
  return ipcm;
}

/* Decode ipcm.
 */
 
struct akau_ipcm *akau_ipcm_decode(const void *src,int srcc) {
  int format=akau_pcm_detect_format(src,srcc);
  switch (format) {
    case AKAU_PCM_FORMAT_WAV: return akau_ipcm_decode_wav(src,srcc);
    case AKAU_PCM_FORMAT_AKAUPCM: return akau_ipcm_decode_akaupcm(src,srcc);
    case AKAU_PCM_FORMAT_WAVEGEN: return akau_wavegen_decode(src,srcc);
    default: {
        akau_error("Unable to detect PCM format.");
        return 0;
      }
  }
}

/* Decode fpcm.
 * In many cases, we can only decode to ipcm.
 * If that's so, do it and then convert to fpcm with sensible defaults.
 */
 
struct akau_fpcm *akau_fpcm_decode(const void *src,int srcc) {
  struct akau_ipcm *ipcm=0;
  
  int format=akau_pcm_detect_format(src,srcc);
  switch (format) {
    case AKAU_PCM_FORMAT_WAV: ipcm=akau_ipcm_decode_wav(src,srcc); break;
    case AKAU_PCM_FORMAT_AKAUPCM: ipcm=akau_ipcm_decode_akaupcm(src,srcc); break;
    case AKAU_PCM_FORMAT_WAVEGEN: ipcm=akau_wavegen_decode(src,srcc); break;
    default: {
        akau_error("Unable to detect PCM format.");
        return 0;
      }
  }

  if (!ipcm) return 0;
  struct akau_fpcm *fpcm=akau_fpcm_from_ipcm(ipcm,32767);
  akau_ipcm_del(ipcm);
  return fpcm;
}
