#include "ps.h"
#include "akgl/akgl.h"

#define PS_MINFONT_SRC_COLW  5
#define PS_MINFONT_SRC_ROWH  7
#define PS_MINFONT_SRC_COLC 16
#define PS_MINFONT_SRC_ROWC  6
#define PS_MINFONT_SRC_ROWP  2
#define PS_MINFONT_SRC_SIZE ((PS_MINFONT_SRC_COLW*PS_MINFONT_SRC_COLC*PS_MINFONT_SRC_ROWH*PS_MINFONT_SRC_ROWC+7)>>3)

#define PS_MINFONT_DST_COLW  6
#define PS_MINFONT_DST_ROWH 12
#define PS_MINFONT_DST_COLC 16
#define PS_MINFONT_DST_ROWC 16
#define PS_MINFONT_DST_X0    0
#define PS_MINFONT_DST_Y0    2
#define PS_MINFONT_DST_TOTALW (PS_MINFONT_DST_COLW*PS_MINFONT_DST_COLC)
#define PS_MINFONT_DST_TOTALH (PS_MINFONT_DST_ROWH*PS_MINFONT_DST_ROWC)

/* 80x42 pixels, 1-bit big-endian.
 * Describes rows 0x20..0x70 of ASCII glyphs.
 * Rows are guaranteed to align to a byte boundary, since there are 16 columns.
 * Each cell is 5x7 pixels. We pad with at least 1 pixel on each side of each cell.
 */
static const uint8_t ps_minfont_data[PS_MINFONT_SRC_SIZE]={
  0x01,0x14,0x02,0x01,0x84,0x12,0x08,0x00,0x00,0x00,0x01,0x14,0xa7,0x46,0x44,0x21,
  0x2a,0x40,0x00,0x01,0x01,0x01,0xf4,0x0a,0x80,0x40,0x9c,0x40,0x00,0x02,0x01,0x00,
  0xa7,0x13,0x00,0x40,0x89,0xf0,0x7c,0x04,0x01,0x01,0xf1,0x22,0xa0,0x40,0x9c,0x40,
  0x00,0x08,0x00,0x00,0xa7,0x46,0x40,0x21,0x2a,0x46,0x00,0x90,0x01,0x00,0x02,0x01,
  0xa0,0x12,0x08,0x02,0x00,0x00,0x71,0x3d,0xe8,0xfd,0xdf,0x73,0xc0,0x01,0x02,0x0e,
  0x8b,0x02,0x18,0xc2,0x01,0x8c,0x40,0x02,0x01,0x11,0x9d,0x02,0x18,0xc2,0x01,0x8c,
  0x48,0x04,0x7c,0x81,0xa9,0x1c,0x6f,0xfb,0xc1,0x73,0xc0,0x48,0x00,0x46,0xc9,0x20,
  0x10,0x86,0x21,0x88,0x48,0x04,0x7c,0x84,0x89,0x20,0x10,0x86,0x21,0x88,0x40,0xc2,
  0x01,0x00,0x77,0xff,0xe0,0xf9,0xc1,0x70,0x40,0x41,0x02,0x04,0x73,0xbc,0xee,0x7f,
  0xee,0x8f,0xc3,0x18,0x46,0x2e,0x8c,0x63,0x19,0x42,0x10,0x89,0x03,0x28,0x6f,0x31,
  0xac,0x63,0x08,0xc2,0x10,0x89,0x03,0x48,0x57,0x31,0xaf,0xfd,0x08,0xfb,0xd7,0xf9,
  0x03,0x88,0x46,0xb1,0xbc,0x63,0x08,0xc2,0x11,0x89,0x23,0x48,0x46,0x71,0x84,0x63,
  0x19,0x42,0x11,0x89,0x23,0x28,0x46,0x71,0x7c,0x7c,0xee,0x7e,0x0f,0x8f,0xdd,0x1f,
  0xc6,0x2e,0xf3,0xbc,0xff,0xc6,0x31,0x8c,0x7e,0xe0,0x38,0x80,0x8c,0x63,0x02,0x46,
  0x31,0x8c,0x42,0x88,0x09,0x40,0x8c,0x63,0x02,0x46,0x31,0x54,0x44,0x84,0x0a,0x20,
  0xf4,0x7c,0xe2,0x45,0x51,0x23,0x88,0x82,0x08,0x00,0x85,0x68,0x12,0x45,0x55,0x51,
  0x10,0x81,0x08,0x00,0x84,0xe4,0x12,0x44,0x9b,0x89,0x20,0x80,0x88,0x00,0x83,0xe3,
  0xe2,0x38,0x91,0x89,0x3e,0xe0,0x38,0x1f,0x40,0x20,0x00,0x80,0x40,0x80,0x00,0x84,
  0x00,0x00,0x20,0x20,0x00,0x80,0x80,0x80,0x08,0x84,0x00,0x00,0x13,0xa0,0x00,0xb8,
  0x8e,0x81,0x00,0x84,0x00,0x00,0x00,0x7c,0xf7,0xc5,0xd1,0xe0,0x08,0xa4,0x6b,0x8e,
  0x03,0xe3,0x08,0xf8,0x8f,0x91,0x08,0xc4,0x56,0x51,0x04,0x63,0x08,0xc0,0x81,0x89,
  0x28,0xa4,0x56,0x31,0x03,0xfc,0xf7,0xbc,0x8e,0x89,0x10,0x92,0x56,0x2e,0x00,0x00,
  0x02,0x00,0x00,0x00,0x00,0x22,0x20,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x42,
  0x10,0x00,0x00,0x00,0xf2,0x00,0x00,0x8c,0x7c,0x42,0x11,0x00,0xf3,0xfd,0x07,0x46,
  0x31,0x54,0x44,0x82,0x0a,0xa0,0x8c,0x62,0xe2,0x46,0x55,0x23,0xc8,0x42,0x10,0x40,
  0xf3,0xe0,0x12,0x46,0x95,0x50,0x50,0x42,0x10,0x00,0x80,0x61,0xe1,0x3b,0x0a,0x8b,
  0x9e,0x22,0x20,0x00
};

static void ps_minfont_copy(uint8_t *dst,const uint8_t *src) {

  /* Skip the first two rows (or whatever PS_MINFONT_SRC_ROWP is). */
  dst+=PS_MINFONT_SRC_ROWP*PS_MINFONT_DST_TOTALW*PS_MINFONT_DST_ROWH;

  /* Set initial position within output row. */
  dst+=PS_MINFONT_DST_X0;
  dst+=PS_MINFONT_DST_Y0*PS_MINFONT_DST_TOTALW;
  uint8_t *dstrowstart=dst; /* Position of first used pixel in dst row. */
  int dstrowstride=PS_MINFONT_DST_TOTALW*PS_MINFONT_DST_ROWH;

  /* We will be iterating over (src) one bit at a time... */
  uint8_t mask=0x80;

  /* Iterate over major rows. */
  int row_major=PS_MINFONT_SRC_ROWC;
  while (row_major-->0) {
    dst=dstrowstart;
    dstrowstart+=dstrowstride;
    uint8_t *dstminorrowstart=dst;

    /* Iterate over minor rows. */
    int row_minor=PS_MINFONT_SRC_ROWH;
    while (row_minor-->0) {
      dst=dstminorrowstart;
      dstminorrowstart+=PS_MINFONT_DST_TOTALW;

      /* Iterate over major columns. */
      int col_major=PS_MINFONT_SRC_COLC;
      while (col_major-->0) {

        /* Iterate over minor columns. */
        int col_minor=PS_MINFONT_SRC_COLW;
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

        dst+=(PS_MINFONT_DST_COLW-PS_MINFONT_SRC_COLW);
      }
    
    }
    
  }
}

int ps_video_load_minfont(struct akgl_texture *texture) {
  if (!texture) return -1;

  if (akgl_texture_set_filter(texture,0)<0) return -1;

  uint8_t *pixels=calloc(PS_MINFONT_DST_TOTALW,PS_MINFONT_DST_TOTALH);
  if (!pixels) return -1;

  ps_minfont_copy(pixels,ps_minfont_data);

  if (akgl_texture_load(texture,pixels,AKGL_FMT_Y8,PS_MINFONT_DST_TOTALW,PS_MINFONT_DST_TOTALH)<0) {
    free(pixels);
    return -1;
  }

  free(pixels);
  return 0;
}
