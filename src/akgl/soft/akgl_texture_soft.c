#include "akgl/akgl_internal.h"

#define AKGL_TEXTURE_SOFT_SIZE_LIMIT 2048

/* Image format properties.
 * We deliberately chose a small set of formats where each pixel aligns to 1 byte, for simplicity.
 */

int akgl_fmt_get_pixel_size(int fmt) {
  switch (fmt) {
    case AKGL_FMT_RGB8: return 3;
    case AKGL_FMT_RGBA8: return 4;
    case AKGL_FMT_Y8: return 1;
    case AKGL_FMT_YA8: return 2;
    case AKGL_FMT_A8: return 1;
  }
  return 0;
}

/* Object lifecycle.
 */
 
struct akgl_texture_soft *akgl_texture_soft_new() {
  struct akgl_texture_soft *tex=calloc(1,sizeof(struct akgl_texture_soft));
  if (!tex) return 0;

  tex->refc=1;
  tex->magic=AKGL_MAGIC_TEXTURE_SOFT;

  return tex;
}

void akgl_texture_soft_del(struct akgl_texture_soft *tex) {
  if (!tex) return;
  if (tex->refc-->0) return;

  if (tex->pixels) free(tex->pixels);

  free(tex);
}

int akgl_texture_soft_ref(struct akgl_texture_soft *tex) {
  if (!tex) return -1;
  if (tex->refc<1) return -1;
  if (tex->refc==INT_MAX) return -1;
  tex->refc++;
  return 0;
}

/* Accessors.
 */

int akgl_texture_soft_get_fmt(const struct akgl_texture_soft *tex) {
  if (!tex) return -1;
  return tex->fmt;
}

int akgl_texture_soft_get_size(int *w,int *h,const struct akgl_texture_soft *tex) {
  if (!tex) return -1;
  if (w) *w=tex->w;
  if (h) *h=tex->h;
  return 0;
}

/* Copy framebuffer to new texture.
 */
 
struct akgl_texture_soft *akgl_texture_soft_from_framebuffer(struct akgl_framebuffer_soft *fb) {
  if (!fb||(fb->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT)) return 0;
  struct akgl_texture_soft *tex=akgl_texture_soft_new();
  if (!tex) return 0;
  if (akgl_texture_soft_load(tex,fb->pixels,AKGL_FMT_RGBA8,fb->w,fb->h)<0) {
    akgl_texture_soft_del(tex);
    return 0;
  }
  return tex;
}

/* Load pixels.
 */

int akgl_texture_soft_load(
  struct akgl_texture_soft *tex,
  const void *pixels,
  int fmt,int w,int h
) {
  if (akgl_texture_soft_realloc(tex,fmt,w,h)<0) return -1;
  if (akgl_texture_soft_load_sub(tex,pixels,0,0,w,h)<0) return -1;
  return 0;
}

/* Reallocate pixels.
 */

int akgl_texture_soft_realloc(
  struct akgl_texture_soft *tex,
  int fmt,int w,int h
) {
  if (!tex) return -1;
  if (tex->magic!=AKGL_MAGIC_TEXTURE_SOFT) return -1;
  if ((fmt==tex->fmt)&&(w==tex->w)&&(h==tex->h)) return 0;
  if ((w<1)||(w>AKGL_TEXTURE_SOFT_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>AKGL_TEXTURE_SOFT_SIZE_LIMIT)) return -1;

  int colstride=akgl_fmt_get_pixel_size(fmt);
  if (colstride<1) return -1;
  int rowstride=colstride*w;

  void *nv=calloc(rowstride,h);
  if (!nv) return -1;
  if (tex->pixels) free(tex->pixels);
  tex->pixels=nv;
  tex->fmt=fmt;
  tex->w=w;
  tex->h=h;

  return 0;
}

/* Load pixels.
 */

int akgl_texture_soft_load_sub(
  struct akgl_texture_soft *tex,
  const void *pixels,
  int x,int y,int w,int h
) {
  if (!tex||!pixels) return -1;
  if (tex->magic!=AKGL_MAGIC_TEXTURE_SOFT) return -1;
  if ((w<1)||(x<0)||(x>tex->w-w)) return -1;
  if ((h<1)||(y<0)||(y>tex->h-h)) return -1;
  
  int colstride=akgl_fmt_get_pixel_size(tex->fmt);
  if (colstride<1) return -1;
  int dstrowstride=colstride*tex->w;
  int srcrowstride=colstride*w;

  const uint8_t *srcrow=pixels;
  uint8_t *dstrow=tex->pixels;
  dstrow+=y*dstrowstride+x*colstride;
  while (h-->0) {
    memcpy(dstrow,srcrow,srcrowstride);
    srcrow+=srcrowstride;
    dstrow+=dstrowstride;
  }

  return 0;
}
