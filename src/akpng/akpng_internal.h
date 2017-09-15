#ifndef AKPNG_INTERNAL_H
#define AKPNG_INTERNAL_H

#include "akpng.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>

/* Filter strategies 0..4 mean always use the named filter. */
#define AKPNG_FILTER_STRATEGY_LONGEST_ZERO_RUN    5
#define AKPNG_FILTER_STRATEGY_MOST_ZEROES         6
#define AKPNG_FILTER_STRATEGY_LONGEST_RUN         7
#define AKPNG_FILTER_STRATEGY_LOWEST_ABS_SUM      8

struct akpng_encoder {
  z_stream z;
  const struct akpng_image *image;
  int xstride;
  int chanc;
  int pixelsize;
  int zinit;
  uint8_t *dst;
  int dstc,dsta;
  int filter_strategy;
  uint8_t *rowbuf;
  int y;
};

struct akpng_decoder {
  z_stream z;
  int zinit;
  struct akpng_image *image;
  int have_ihdr;
  int interlace;
  int chanc;
  int pixelsize;
  int xstride;
  int y;
  uint8_t *rowbuf;
};

void akpng_encoder_cleanup(struct akpng_encoder *encoder);

void akpng_decoder_cleanup(struct akpng_decoder *decoder);

/* Return count of channels for the given type.
 * Also validate that the provided depth is legal for this type.
 * If this passes, we guarantee that (colortype*depth) is a sane and legal pixel size.
 */
static inline int akpng_chanc_for_colortype(int colortype,int depth) {
  switch (colortype) {
    case 0: switch (depth) {
        case 1: case 2: case 4: case 8: case 16: return 1;
      } break;
    case 2: switch (depth) {
        case 8: case 16: return 3;
      } break;
    case 3: switch (depth) {
        case 1: case 2: case 4: case 8: return 1;
      } break;
    case 4: switch (depth) {
        case 8: case 16: return 2;
      } break;
    case 6: switch (depth) {
        case 8: case 16: return 4;
      } break;
  }
  return -1;
}

int akpng_image_chunkv_require(struct akpng_image *image);

/* Copy (src) into (dst), cleaning up old content in (dst).
 * Pixels and header are copied.
 * Extra chunks are preserved in (dst), except PLTE and tRNS.
 * It is assumed that (src) contains no chunks.
 */
void akpng_image_handoff_content(struct akpng_image *dst,const struct akpng_image *src);

struct akpng_iterator {
  int (*read)(struct akpng_iterator *iterator);
  void (*write)(struct akpng_iterator *iterator,int v);
  uint8_t *row;
  int x,y,w,h; // (x,w) are in channels, not pixels.
  int stride;
  int normalize;
// private to (read,write):
  uint8_t *p;
  int shift;
};

int akpng_iterator_setup(struct akpng_iterator *iterator,const struct akpng_image *image,int normalize);

int akpng_perform_filter(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride,int filter);
int akpng_perform_unfilter(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride,int filter);

#endif
