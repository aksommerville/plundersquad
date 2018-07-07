/* ps_video_load_minnerfont.c
 * Smallest possible ASCII font.
 * Upper-case letters only, and it straddles the fence of legibility.
 * We're not using this right now, but keeping it handy in case I change I mind.
 */

#include "ps.h"
#include "akgl/akgl.h"

#define PS_MINNERFONT_SRC_COLW  3
#define PS_MINNERFONT_SRC_ROWH  5
#define PS_MINNERFONT_SRC_COLC 16
#define PS_MINNERFONT_SRC_ROWC  6
#define PS_MINNERFONT_SRC_ROWP  2
#define PS_MINNERFONT_SRC_SIZE ((PS_MINNERFONT_SRC_COLW*PS_MINNERFONT_SRC_COLC*PS_MINNERFONT_SRC_ROWH*PS_MINNERFONT_SRC_ROWC+7)>>3)

#define PS_MINNERFONT_DST_COLW  4
#define PS_MINNERFONT_DST_ROWH  8
#define PS_MINNERFONT_DST_COLC 16
#define PS_MINNERFONT_DST_ROWC 16
#define PS_MINNERFONT_DST_X0    0
#define PS_MINNERFONT_DST_Y0    1
#define PS_MINNERFONT_DST_TOTALW (PS_MINNERFONT_DST_COLW*PS_MINNERFONT_DST_COLC)
#define PS_MINNERFONT_DST_TOTALH (PS_MINNERFONT_DST_ROWH*PS_MINNERFONT_DST_ROWC)

static const uint8_t ps_minnerfont_data[PS_MINNERFONT_SRC_SIZE]={
  0x0a,0xdf,0x12,0x33,0x80,0x00,0x0a,0xfc,0x62,0x4b,0xa0,0x01,
  0x08,0x5e,0xb0,0x4b,0xf1,0xc2,0x00,0x77,0x28,0x48,0x2c,0x04,
  0x08,0x5e,0x78,0x30,0x04,0x10,0x5b,0x6b,0xd7,0x4c,0x02,0x26,
  0xa8,0x9b,0x21,0xb5,0x25,0xd1,0xa9,0x2f,0xb1,0x4c,0x08,0x0a,
  0xaa,0x12,0x69,0xa5,0x65,0xd0,0x5f,0xe3,0x91,0x44,0x22,0x22,
  0xeb,0x3d,0xfb,0xbc,0xd9,0xca,0xb6,0xcb,0x24,0xa8,0xd9,0xcd,
  0xff,0x4b,0xb5,0xea,0xe9,0xfd,0x96,0xcb,0x25,0xaa,0xd9,0x65,
  0xf7,0x3d,0xe3,0xbd,0x5f,0x62,0xcb,0x3f,0x6d,0xb7,0xb1,0x90,
  0xb6,0xc5,0x6d,0xb4,0xa8,0xa8,0xd7,0x25,0x6f,0x49,0x24,0x80,
  0x9e,0x95,0x7f,0xaa,0x22,0x80,0x8e,0xe5,0xd7,0xab,0xb1,0x87,
  0xcb,0x3d,0xfb,0xbc,0xd9,0xca,0x76,0xcb,0x24,0xa8,0xd9,0xcd,
  0x1f,0x4b,0xb5,0xea,0xe9,0xfd,0x16,0xcb,0x25,0xaa,0xd9,0x65,
  0x17,0x3d,0xe3,0xbd,0x5f,0x62,0xcb,0x3f,0x6d,0xb7,0xb5,0x98,
  0xb6,0xc5,0x6d,0xb4,0xa4,0xb0,0xd7,0x25,0x6f,0x49,0x44,0x40,
  0x9e,0x95,0x7f,0xaa,0x24,0x80,0x8e,0xe5,0xd7,0xab,0xb5,0x80,
};

static void ps_minnerfont_copy(uint8_t *dst,const uint8_t *src) {

  /* Skip the first two rows (or whatever PS_MINNERFONT_SRC_ROWP is). */
  dst+=PS_MINNERFONT_SRC_ROWP*PS_MINNERFONT_DST_TOTALW*PS_MINNERFONT_DST_ROWH;

  /* Set initial position within output row. */
  dst+=PS_MINNERFONT_DST_X0;
  dst+=PS_MINNERFONT_DST_Y0*PS_MINNERFONT_DST_TOTALW;
  uint8_t *dstrowstart=dst; /* Position of first used pixel in dst row. */
  int dstrowstride=PS_MINNERFONT_DST_TOTALW*PS_MINNERFONT_DST_ROWH;

  /* We will be iterating over (src) one bit at a time... */
  uint8_t mask=0x80;

  /* Iterate over major rows. */
  int row_major=PS_MINNERFONT_SRC_ROWC;
  while (row_major-->0) {
    dst=dstrowstart;
    dstrowstart+=dstrowstride;
    uint8_t *dstminorrowstart=dst;

    /* Iterate over minor rows. */
    int row_minor=PS_MINNERFONT_SRC_ROWH;
    while (row_minor-->0) {
      dst=dstminorrowstart;
      dstminorrowstart+=PS_MINNERFONT_DST_TOTALW;

      /* Iterate over major columns. */
      int col_major=PS_MINNERFONT_SRC_COLC;
      while (col_major-->0) {

        /* Iterate over minor columns. */
        int col_minor=PS_MINNERFONT_SRC_COLW;
        while (col_minor-->0) {
          if (*src&mask) {
            *dst=0xff;
          }
          dst++;
          if (!(mask>>=1)) {
            mask=0x80;
            src++;
          }
        }

        dst+=(PS_MINNERFONT_DST_COLW-PS_MINNERFONT_SRC_COLW);
      }
    
    }
    
  }
}

int ps_video_load_minnerfont(struct akgl_texture *texture) {
  if (!texture) return -1;

  if (akgl_texture_set_filter(texture,0)<0) return -1;

  uint8_t *pixels=calloc(PS_MINNERFONT_DST_TOTALW,PS_MINNERFONT_DST_TOTALH);
  if (!pixels) return -1;

  ps_minnerfont_copy(pixels,ps_minnerfont_data);

  if (akgl_texture_load(texture,pixels,AKGL_FMT_Y8,PS_MINNERFONT_DST_TOTALW,PS_MINNERFONT_DST_TOTALH)<0) {
    free(pixels);
    return -1;
  }

  free(pixels);
  return 0;
}
