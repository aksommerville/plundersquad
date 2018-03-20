/* ps_sdraw.h
 * Software rendering.
 */

#ifndef PS_SDRAW_H
#define PS_SDRAW_H

struct akgl_vtx_maxtile;

#define PS_SDRAW_FMT_RGB    1
#define PS_SDRAW_FMT_RGBX   2
#define PS_SDRAW_FMT_RGBA   3
#define PS_SDRAW_FMT_A      4

struct ps_sdraw_image {
  int refc;
  int fmt;
  int w,h;
  int colstride,rowstride; // Knowable from (fmt,w)
  uint8_t *pixels;
};

/* Pixel formats.
 ******************************************************************/

struct ps_sdraw_rgba {
  uint8_t r,g,b,a;
};

static inline struct ps_sdraw_rgba ps_sdraw_rgba(uint8_t r,uint8_t g,uint8_t b,uint8_t a) {
  return (struct ps_sdraw_rgba){r,g,b,a};
}
static inline struct ps_sdraw_rgba ps_sdraw_rgba32(uint32_t rgba) {
  return (struct ps_sdraw_rgba){rgba>>24,rgba>>16,rgba>>8,rgba};
}

int ps_sdraw_fmt_has_rgb(int fmt);
int ps_sdraw_fmt_has_alpha(int fmt);
int ps_sdraw_fmt_has_luma(int fmt);

/* All formats are byte-aligned.
 * This returns the size of one pixel in bytes, or zero.
 */
int ps_sdraw_fmt_pixel_size(int fmt);

/* 'pxrd' reads a pixel to general r,g,b,a.
 * 'pxcp' writes a pixel verbatim.
 * 'pxwr' blends with the incoming alpha if warranted.
 */
typedef struct ps_sdraw_rgba (*ps_sdraw_pxrd_fn)(const uint8_t *src);
typedef void (*ps_sdraw_pxwr_fn)(uint8_t *dst,struct ps_sdraw_rgba rgba);
typedef void (*ps_sdraw_pxcp_fn)(uint8_t *dst,struct ps_sdraw_rgba rgba);

ps_sdraw_pxrd_fn ps_sdraw_pxrd_for_fmt(int fmt);
ps_sdraw_pxwr_fn ps_sdraw_pxwr_for_fmt(int fmt);
ps_sdraw_pxcp_fn ps_sdraw_pxcp_for_fmt(int fmt);

/* Image object.
 *****************************************************************/

struct ps_sdraw_image *ps_sdraw_image_new();
void ps_sdraw_image_del(struct ps_sdraw_image *image);
int ps_sdraw_image_ref(struct ps_sdraw_image *image);

/* Successful realloc zeroes the pixels, even if dimensions unchanged.
 */
int ps_sdraw_image_realloc(struct ps_sdraw_image *image,int fmt,int w,int h);

int ps_sdraw_image_load(struct ps_sdraw_image *image,const void *pixels,int fmt,int w,int h);

int ps_sdraw_image_load_sub(struct ps_sdraw_image *image,const void *pixels,int x,int y,int w,int h);

/* Rendering.
 *****************************************************************/

int ps_sdraw_draw_rect(struct ps_sdraw_image *image,int x,int y,int w,int h,struct ps_sdraw_rgba rgba);

/* Ordinary blit.
 * Dimensions must be provided for both source and destination; we scale automatically.
 * If the source image has an alpha channel, we use it.
 */
int ps_sdraw_blit(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch
);

/* Blit for text.
 * Ignore chroma in the source image, replace with (rgba).
 */
int ps_sdraw_blit_replacergb(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch,
  struct ps_sdraw_rgba rgba
);

/* Blit for sprites.
 * (src) must be a full 16x16-tile spritesheet.
 * We acknowledge all fields in (vtx), just as if we were AKGL.
 */
int ps_sdraw_blit_maxtile(
  struct ps_sdraw_image *dst,
  const struct akgl_vtx_maxtile *vtx,
  const struct ps_sdraw_image *src
);

#endif
