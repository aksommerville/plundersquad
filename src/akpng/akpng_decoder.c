#include "akpng_internal.h"

#define RD32(src) ( \
  (((uint8_t*)(src))[0]<<24)| \
  (((uint8_t*)(src))[1]<<16)| \
  (((uint8_t*)(src))[2]<<8)| \
  (((uint8_t*)(src))[3]) \
)

/* Clean up context.
 */

void akpng_decoder_cleanup(struct akpng_decoder *decoder) {
  if (!decoder) return;
  if (decoder->zinit) inflateEnd(&decoder->z);
  if (decoder->rowbuf) free(decoder->rowbuf);
}

/* Filtered row.
 */

static int akpng_decode_row(struct akpng_decoder *decoder) {
  uint8_t *dst=(uint8_t*)decoder->image->pixels+decoder->y*decoder->image->stride;
  uint8_t *prv=0;
  if (decoder->y) prv=dst-decoder->image->stride;
  if (akpng_perform_unfilter(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride,decoder->rowbuf[0])<0) return -1;
  return 0;
}

/* IDAT.
 * Spec requires that all IDAT be contiguous, no intervening chunks.
 * I see no reason to enforce that.
 */

static int akpng_decode_idat(struct akpng_decoder *decoder,const uint8_t *src,int srcc) {
  if (!decoder->have_ihdr) return -1;
  if (decoder->y>=decoder->image->h) return 0; // Ignore excess data.

  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in) {
    if (!decoder->z.avail_out) {
      if (akpng_decode_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
      decoder->y++;
      if (decoder->y>=decoder->image->h) break;
    }
    int err=inflate(&decoder->z,Z_NO_FLUSH);
    if (err<0) {
      err=inflate(&decoder->z,Z_SYNC_FLUSH);
      if (err<0) return -1;
    }
  }

  return 0;
}

/* IHDR.
 * Technically, IHDR is required to be the first chunk.
 * I see no reason to enforce that, and in fact I've seen PNGs violate that rule.
 * Only thing is it must come before any IDAT.
 */

static int akpng_decode_ihdr(struct akpng_decoder *decoder,const uint8_t *src,int srcc) {
  if (decoder->have_ihdr) return -1;
  if (srcc<13) return -1; // Spec says exactly 13 only, but whatever.
  decoder->have_ihdr=1;
  
  int w=RD32(src);
  int h=RD32(src+4);
  int depth=src[8];
  int colortype=src[9];
  int compression=src[10];
  int filter=src[11];
  int interlace=src[12];

  if ((w<1)||(h<1)) return -1;
  if (w>INT_MAX/h) return -1;
  if ((decoder->chanc=akpng_chanc_for_colortype(colortype,depth))<1) return -1;
  decoder->pixelsize=decoder->chanc*depth;
  decoder->xstride=(decoder->pixelsize+7)>>3;

  if (compression||filter) return -1;
  if (interlace) return -1; // We don't support interlacing; it's obsolete and complicated.

  if (akpng_image_realloc(decoder->image,w,h,depth,colortype)<0) return -1;

  if (!(decoder->rowbuf=malloc(1+decoder->image->stride))) return -1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=1+decoder->image->stride;

  return 0;
}

/* Other chunk, not (IHDR,IDAT,IEND).
 */

static int akpng_decode_other(struct akpng_decoder *decoder,const char *type,const uint8_t *src,int srcc) {
  if (!decoder->have_ihdr) return -1;
  // We violate the spec here by not checking the "required" bit.
  return akpng_image_create_chunk(decoder->image,type,src,srcc);
}

/* Assert completion.
 */

static int akpng_decode_finish(struct akpng_decoder *decoder) {
  if (!decoder->have_ihdr) return -1;

  /* Drain zlib context. */
  int zend=0;
  while (decoder->y<decoder->image->h) {
    if (!decoder->z.avail_out) {
      if (akpng_decode_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
      decoder->y++;
      if (decoder->y>=decoder->image->h) break;
    }
    if (zend) break;
    int err=inflate(&decoder->z,Z_FINISH);
    if (err<0) return -1;
    if (err==Z_STREAM_END) zend=1;
  }
  if (decoder->y<decoder->image->h) return -1;
  
  // The spec does technically require an IEND chunk; we don't enforce that.
  // We could check whether PLTE is present for colortype==3, but really we are tolerant of missing PLTE.

  return 0;
}

/* Decode all, with context ready.
 */

static int akpng_decode_all(struct akpng_decoder *decoder,const uint8_t *src,int srcc) {

  if (srcc<8) return -1;
  if (memcmp(src,"\x89PNG\r\n\x1a\n",8)) return -1;

  int srcp=8;
  while (1) {
  
    if (srcp>srcc-8) return -1;
    int chunklen=RD32(src+srcp); srcp+=4;
    const char *type=(char*)src+srcp; srcp+=4;
    if (chunklen<0) return -1;
    if (srcp>srcc-chunklen) return -1;
    const uint8_t *chunk=src+srcp;
    srcp+=chunklen;
    if (srcp>srcc-4) return -1;
    srcp+=4; // Don't bother validating CRCs

    if (!memcmp(type,"IEND",4)) {
      break;
    } else if (!memcmp(type,"IHDR",4)) {
      if (akpng_decode_ihdr(decoder,chunk,chunklen)<0) return -1;
    } else if (!memcmp(type,"IDAT",4)) {
      if (akpng_decode_idat(decoder,chunk,chunklen)<0) return -1;
    } else {
      if (akpng_decode_other(decoder,type,chunk,chunklen)<0) return -1;
    }
  }

  return akpng_decode_finish(decoder);
}

/* Decode, main entry point.
 */

int akpng_decode(struct akpng_image *image,const void *src,int srcc) {
  if (!image||!src||(srcc<0)) return -1;
  akpng_image_cleanup(image);

  struct akpng_decoder decoder={
    .image=image,
  };
  if (inflateInit(&decoder.z)<0) {
    akpng_decoder_cleanup(&decoder);
    return -1;
  }
  decoder.zinit=1;

  int err=akpng_decode_all(&decoder,src,srcc);
  akpng_decoder_cleanup(&decoder);
  if (err<0) akpng_image_cleanup(image);
  return err;
}
