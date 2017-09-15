#include "akpng_internal.h"

#define WR32(dst,src) { \
  int WR32_src=(src); \
  ((uint8_t*)(dst))[0]=(WR32_src)>>24; \
  ((uint8_t*)(dst))[1]=(WR32_src)>>16; \
  ((uint8_t*)(dst))[2]=(WR32_src)>>8; \
  ((uint8_t*)(dst))[3]=(WR32_src); \
}

/* Clean up.
 */

void akpng_encoder_cleanup(struct akpng_encoder *encoder) {
  if (!encoder) return;
  if (encoder->zinit) deflateEnd(&encoder->z);
  if (encoder->dst) free(encoder->dst);
  if (encoder->rowbuf) free(encoder->rowbuf);
}

/* Grow internal buffer.
 */

static int akpng_encoder_dst_require(struct akpng_encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->dstc>INT_MAX-addc) return -1;
  int na=encoder->dstc+addc;
  if (na<=encoder->dsta) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->dst,na);
  if (!nv) return -1;
  encoder->dst=nv;
  encoder->dsta=na;
  return 0;
}

/* Append to internal buffer.
 */

static int akpng_encoder_append(struct akpng_encoder *encoder,const void *src,int srcc) {
  if (srcc<0) return -1;
  if (akpng_encoder_dst_require(encoder,srcc)<0) return -1;
  memcpy(encoder->dst+encoder->dstc,src,srcc);
  encoder->dstc+=srcc;
  return 0;
}

static int akpng_encoder_append_chunk(struct akpng_encoder *encoder,const char *type,const void *src,int srcc) {
  if (srcc<0) return -1;
  uint8_t tmp[4];
  WR32(tmp,srcc);
  if (akpng_encoder_append(encoder,tmp,4)<0) return -1;
  int startp=encoder->dstc;
  if (akpng_encoder_append(encoder,type,4)<0) return -1;
  if (akpng_encoder_append(encoder,src,srcc)<0) return -1;
  int crc=crc32(crc32(0,0,0),encoder->dst+startp,encoder->dstc-startp);
  WR32(tmp,crc);
  if (akpng_encoder_append(encoder,tmp,4)<0) return -1;
  return 0;
}

/* Filter selection primitives.
 */

static int akpng_filter_score_longest_zero_run(const uint8_t *src,int srcc) {
  int longest=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]) { srcp++; continue; }
    srcp++;
    int runlen=1;
    while ((srcp<srcc)&&!src[srcp]) { srcp++; runlen++; }
    if (runlen>longest) longest=runlen;
  }
  return longest;
}

static int akpng_filter_score_most_zeroes(const uint8_t *src,int srcc) {
  int zeroc=0,srcp=0;
  for (;srcp<srcc;srcp++) if (!src[srcp]) zeroc++;
  return zeroc;
}

static int akpng_filter_score_longest_run(const uint8_t *src,int srcc) {
  int longest=0,srcp=0;
  while (srcp<srcc) {
    uint8_t q=src[srcp++];
    int runlen=1;
    while ((srcp<srcc)&&(src[srcp]==q)) { srcp++; runlen++; }
    if (runlen>longest) longest=runlen;
  }
  return longest;
}

static int akpng_filter_score_lowest_abs_sum(const uint8_t *src,int srcc) {
  int sum=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    int8_t d=src[srcp];
    sum+=d;
  }
  return sum;
}

/* Select and commit a row filter.
 */

static uint8_t akpng_filter_row(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride,int strategy) {
  if (strategy>=5) {
    int bestscore=0,bestfilter=0;
    int i=0; for (;i<5;i++) {
      akpng_perform_filter(dst,src,prv,stride,xstride,i);
      int score;
      switch (strategy) {
        case AKPNG_FILTER_STRATEGY_LONGEST_ZERO_RUN: score=akpng_filter_score_longest_zero_run(dst,stride); break;
        case AKPNG_FILTER_STRATEGY_MOST_ZEROES: score=akpng_filter_score_most_zeroes(dst,stride); break;
        case AKPNG_FILTER_STRATEGY_LONGEST_RUN: score=akpng_filter_score_longest_run(dst,stride); break;
        case AKPNG_FILTER_STRATEGY_LOWEST_ABS_SUM: score=akpng_filter_score_lowest_abs_sum(dst,stride); break;
        default: return i; // Invalid strategy, default to no filter (0).
      }
      if (score>bestscore) {
        bestscore=score;
        bestfilter=i;
      }
    }
    if (bestfilter!=4) akpng_perform_filter(dst,src,prv,stride,xstride,i);
    return bestfilter;
  } else {
    if (akpng_perform_filter(dst,src,prv,stride,xstride,strategy)<0) {
      akpng_perform_filter(dst,src,prv,stride,xstride,0);
      return 0;
    }
    return strategy;
  }
}

/* Encode one row of image data.
 */

static int akpng_encode_row(struct akpng_encoder *encoder) {

  const uint8_t *src=(uint8_t*)encoder->image->pixels+encoder->image->stride*encoder->y;
  const uint8_t *prv=0;
  if (encoder->y) prv=src-encoder->image->stride;
  encoder->rowbuf[0]=akpng_filter_row(encoder->rowbuf+1,src,prv,encoder->image->stride,encoder->xstride,encoder->filter_strategy);

  encoder->z.next_in=(Bytef*)encoder->rowbuf;
  encoder->z.avail_in=1+encoder->image->stride;
  while (encoder->z.avail_in) {
    if (akpng_encoder_dst_require(encoder,1)<0) return -1;
    encoder->z.next_out=(Bytef*)encoder->dst+encoder->dstc;
    encoder->z.avail_out=encoder->dsta-encoder->dstc;
    int avail_out_0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_NO_FLUSH);
    if (err<0) {
      err=deflate(&encoder->z,Z_SYNC_FLUSH);
      if (err<0) {
        return -1;
      }
    }
    int addc=avail_out_0-encoder->z.avail_out;
    encoder->dstc+=addc;
  }

  return 0;
}

/* After encoding all image data, flush the zlib context into output.
 */

static int akpng_encode_finish_image(struct akpng_encoder *encoder) {
  encoder->z.next_in=0;
  encoder->z.avail_in=0;
  while (1) {
    if (akpng_encoder_dst_require(encoder,1)<0) return -1;
    encoder->z.next_out=(Bytef*)encoder->dst+encoder->dstc;
    encoder->z.avail_out=encoder->dsta-encoder->dstc;
    int avail_out_0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_FINISH);
    if (err<0) {
      return -1;
    }
    int addc=avail_out_0-encoder->z.avail_out;
    encoder->dstc+=addc;
    if (err==Z_STREAM_END) break;
  }
  return 0;
}

/* Encode image data.
 * We use a single IDAT chunk. Is there a good reason to break it down more?
 */

static int akpng_encode_image(struct akpng_encoder *encoder) {

  int lenp=encoder->dstc;
  if (akpng_encoder_append(encoder,"\0\0\0\0",4)<0) return -1;
  int typep=encoder->dstc;
  if (akpng_encoder_append(encoder,"IDAT",4)<0) return -1;
  int chunkp=encoder->dstc;

  for (;encoder->y<encoder->image->h;encoder->y++) {
    if (akpng_encode_row(encoder)<0) return -1;
  }
  if (akpng_encode_finish_image(encoder)<0) return -1;

  int len=encoder->dstc-chunkp;
  WR32(encoder->dst+lenp,len)

  uint8_t tmp[4];
  int crc=crc32(crc32(0,0,0),encoder->dst+typep,encoder->dstc-typep);
  WR32(tmp,crc)
  if (akpng_encoder_append(encoder,tmp,4)<0) return -1;

  return 0;
}

/* Encode auxiliary chunks.
 */

static int akpng_encode_auxiliary(struct akpng_encoder *encoder) {
  const struct akpng_chunk *chunk=encoder->image->chunkv;
  int i=encoder->image->chunkc; for (;i-->0;chunk++) {
    if (akpng_encoder_append_chunk(encoder,chunk->type,chunk->v,chunk->c)<0) return -1;
  }
  return 0;
}

/* Encode IHDR.
 */

static int akpng_encode_ihdr(struct akpng_encoder *encoder) {
  uint8_t ihdr[13];
  WR32(ihdr,encoder->image->w)
  WR32(ihdr+4,encoder->image->h)
  ihdr[8]=encoder->image->depth;
  ihdr[9]=encoder->image->colortype;
  ihdr[10]=0; // compression
  ihdr[11]=0; // filter
  ihdr[12]=0; // interlace
  return akpng_encoder_append_chunk(encoder,"IHDR",ihdr,13);
}

/* Encode IEND.
 * Since IEND is always empty, this is just a matter of encoding static text with the precalculated CRC.
 */

static int akpng_encode_iend(struct akpng_encoder *encoder) {
  return akpng_encoder_append(encoder,"\0\0\0\0IEND\xae\x42\x60\x82",12);
}

/* Encode, main entry point.
 */
 
int akpng_encode(void *dstpp,const struct akpng_image *image) {

  /* Validate arguments aggressively. */
  if (!dstpp||!image) return -1;
  if (!image->pixels) return -1;
  struct akpng_encoder encoder={0};
  encoder.chanc=akpng_chanc_for_colortype(image->colortype,image->depth);
  if (encoder.chanc<1) return -1;
  if (image->w<1) return -1;
  if (image->h<1) return -1;
  encoder.pixelsize=encoder.chanc*image->depth;
  encoder.xstride=(encoder.pixelsize+7)>>3;
  if (image->w>INT_MAX/encoder.xstride) return -1;
  if (image->stride!=image->w*encoder.xstride) return -1;
  if (image->h>INT_MAX/image->stride) return -1;
  encoder.image=image;

  /* Create zlib context. */
  int err=deflateInit(&encoder.z,Z_BEST_COMPRESSION);
  if (err<0) return -1;
  encoder.zinit=1;

  /* Create row buffer. 1+stride bytes. Holds one row plus the filter byte. */
  if (!(encoder.rowbuf=malloc(1+image->stride))) { err=-1; goto _done_; }

  /* Produce chunks. */
  if ((err=akpng_encoder_append(&encoder,"\x89PNG\r\n\x1a\n",8))<0) goto _done_;
  if ((err=akpng_encode_ihdr(&encoder))<0) goto _done_;
  if ((err=akpng_encode_auxiliary(&encoder))<0) goto _done_;
  if ((err=akpng_encode_image(&encoder))<0) goto _done_;
  if ((err=akpng_encode_iend(&encoder))<0) goto _done_;

_done_:
  if (err<0) {
    akpng_encoder_cleanup(&encoder);
    return -1;
  }
  *(void**)dstpp=encoder.dst;
  encoder.dst=0;
  akpng_encoder_cleanup(&encoder);
  return encoder.dstc;
}
