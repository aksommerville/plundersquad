#include "akgl/akgl_internal.h"

/* Reassmble pixel channels.
 */

static inline uint32_t akgl_pixel_from_rgba(uint32_t rgba) {
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  return (a<<24)|(b<<16)|(g<<8)|r;
}

static inline uint32_t akgl_rgba_from_pixel(uint32_t pixel) {
  uint8_t a=pixel>>24,b=pixel>>16,g=pixel>>8,r=pixel;
  return (r<<24)|(g<<16)|(b<<8)|a;
}

/* Transfer a single pixel from texture to framebuffer.
 */

static void akgl_xfer_rgb8(uint32_t *dst,const uint8_t *src) {
  *dst=0xff000000|(src[2]<<16)|(src[1]<<8)|src[0];
}

static void akgl_xfer_rgba8(uint32_t *dst,const uint8_t *src) {
  uint8_t srca=src[3];
  if (srca==0) return;
  if (srca==0xff) {
    *dst=0xff000000|(src[2]<<16)|(src[1]<<8)|src[0];
  } else {
    uint8_t dsta=0xff-srca;
    int r=((*dst&0xff)*dsta+src[0]*srca)>>8;
    int g=(((*dst>>8)&0xff)*dsta+src[1]*srca)>>8;
    int b=(((*dst>>16)&0xff)*dsta+src[2]*srca)>>8;
    *dst=0xff000000|(b<<16)|(g<<8)|r;
  }
}

static void akgl_xfer_y8(uint32_t *dst,const uint8_t *src) {
  *dst=0xff000000|(src[0]<<16)|(src[0]<<8)|src[0];
}

static void akgl_xfer_ya8(uint32_t *dst,const uint8_t *src) {
  uint8_t srca=src[1];
  if (!srca) return;
  if (srca==0xff) {
    *dst=0xff000000|(src[0]<<16)|(src[0]<<8)|src[0];
  } else {
    uint8_t dsta=0xff-srca;
    int srcy=src[0]*srca;
    int r=((*dst&0xff)*dsta+srcy)>>8;
    int g=(((*dst>>8)&0xff)*dsta+srcy)>>8;
    int b=(((*dst>>16)&0xff)*dsta+srcy)>>8;
    *dst=0xff000000|(b<<16)|(g<<8)|r;
  }
}

typedef void (*akgl_xfer_fn)(uint32_t *dst,const uint8_t *src);

static akgl_xfer_fn akgl_xfer_fn_for_fmt(int fmt) {
  switch (fmt) {
    case AKGL_FMT_RGB8: return akgl_xfer_rgb8;
    case AKGL_FMT_RGBA8: return akgl_xfer_rgba8;
    case AKGL_FMT_Y8: return akgl_xfer_y8;
    case AKGL_FMT_YA8: return akgl_xfer_ya8;
    case AKGL_FMT_A8: return akgl_xfer_y8;
    default: return 0;
  }
}

/* Fill rectangle without alpha, boundaries already clipped.
 */

static void akgl_soft_draw_rect_no_alpha(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba) {
  rgba=akgl_pixel_from_rgba(rgba);
  uint32_t *dstrow=fb->pixels+y*fb->w+x;
  while (h-->0) {
    uint32_t *dst=dstrow;
    dstrow+=fb->w;
    int i=w; for (;i-->0;dst++) *dst=rgba;
  }
}

/* Overlay rectangle with solid color and alpha, boundaries already clipped.
 */

static void akgl_soft_draw_rect_alpha(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba) {

  int srcr=(rgba>>24)&0xff,srcg=(rgba>>16)&0xff,srcb=(rgba>>8)&0xff,srca=rgba&0xff;
  if (!srca) return;
  srcr*=srca;
  srcg*=srcg;
  srcb*=srcb;
  uint8_t dsta=0xff-srca;
  
  uint32_t *dstrow=fb->pixels+y*fb->w+x;
  while (h-->0) {
    uint32_t *dst=dstrow;
    dstrow+=fb->w;
    int i=w; for (;i-->0;dst++) {
      int r=((*dst&0xff)*dsta+srcr)>>8;
      int g=(((*dst>>8)&0xff)*dsta+srcg)>>8;
      int b=(((*dst>>16)&0xff)*dsta+srcb)>>8;
      int a=(*dst>>24)+srca;
      if (r>0xff) r=0xff;
      if (g>0xff) g=0xff;
      if (b>0xff) b=0xff;
      if (a>0xff) a=0xff;
      *dst=(a<<24)|(b<<16)|(g<<8)|r;
    }
  }
}

/* Plain rectangle.
 */
 
int akgl_soft_draw_rect(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba) {
  if (!fb||(fb->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT)) return -1;
  if ((w<1)||(h<1)) return 0;
  if (x<0) {
    if ((w+=x)<1) return 0;
    x=0;
  }
  if (y<0) {
    if ((h+=y)<1) return 0;
    y=0;
  }
  if (x>fb->w-w) {
    if ((w=fb->w-x)<1) return 0;
  }
  if (y>fb->h-h) {
    if ((h=fb->h-y)<1) return 0;
  }

  if ((rgba&0xff)==0xff) {
    akgl_soft_draw_rect_no_alpha(fb,x,y,w,h,rgba);
  } else {
    akgl_soft_draw_rect_alpha(fb,x,y,w,h,rgba);
  }
  
  return 0;
}

/* Gradient rectangles.
 */
 
int akgl_soft_draw_vert_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba_n,uint32_t rgba_s) {
  return 0;//TODO gradient
}

int akgl_soft_draw_horz_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba_w,uint32_t rgba_e) {
  return 0;//TODO gradient
}

int akgl_soft_draw_2d_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t nw,uint32_t ne,uint32_t sw,uint32_t se) {
  return 0;//TODO gradient
}

/* Blit from texture, no bells or whistles.
 */
 
int akgl_framebuffer_soft_blit(
  struct akgl_framebuffer_soft *dst,int dstx,int dsty,int dstw,int dsth,
  const struct akgl_texture_soft *src,int srcx,int srcy,int srcw,int srch
) {
  if (!dst||!src) return -1;
  if (dstx<0) { if ((dstw+=dstx)<1) return 0; }
  if (dsty<0) { if ((dsth+=dsty)<1) return 0; }
  if (srcx<0) { if ((srcw+=srcx)<1) return 0; }
  if (srcy<0) { if ((srch+=srcy)<1) return 0; }
  if (dstx>dst->w-dstw) { if ((dstw=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-dsth) { if ((dsth=dst->h-dsty)<1) return 0; }
  if (srcx>src->w-srcw) { if ((srcw=src->w-srcx)<1) return 0; }
  if (srcy>src->h-srch) { if ((srch=src->h-srcy)<1) return 0; }

  int srccolstride=akgl_fmt_get_pixel_size(src->fmt);
  if (srccolstride<1) return -1;
  int srcrowstride=srccolstride*src->w;

  akgl_xfer_fn xfer=akgl_xfer_fn_for_fmt(src->fmt);
  if (!xfer) return -1;

  //TODO blit with scaling
  if (srcw>dstw) srcw=dstw;
  else if (srcw<dstw) dstw=srcw;
  if (srch>dsth) srch=dsth;
  else if (srch<dsth) dsth=srch;

  uint32_t *dstrow=dst->pixels+dsty*dst->w+dstx;
  uint8_t *srcrow=(uint8_t*)src->pixels+srcy*srcrowstride+srcx*srccolstride;
  while (dsth-->0) {
    uint8_t *srcp=srcrow;
    uint32_t *dstp=dstrow;
    dstrow+=dst->w;
    srcrow+=srcrowstride;
    int i=dstw; for (;i-->0;srcp+=srccolstride,dstp++) {
      xfer(dstp,srcp);
    }
  }

  return 0;
}

/* Blit from texture, replacing color.
 */
 
int akgl_framebuffer_soft_blit_replacergb(
  struct akgl_framebuffer_soft *dst,int dstx,int dsty,int dstw,int dsth,
  const struct akgl_texture_soft *src,int srcx,int srcy,int srcw,int srch,
  uint32_t rgba
) {
  if (!dst||!src) return -1;
  if (dstx<0) { if ((dstw+=dstx)<1) return 0; }
  if (dsty<0) { if ((dsth+=dsty)<1) return 0; }
  if (srcx<0) { if ((srcw+=srcx)<1) return 0; }
  if (srcy<0) { if ((srch+=srcy)<1) return 0; }
  if (dstx>dst->w-dstw) { if ((dstw=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-dsth) { if ((dsth=dst->h-dsty)<1) return 0; }
  if (srcx>src->w-srcw) { if ((srcw=src->w-srcx)<1) return 0; }
  if (srcy>src->h-srch) { if ((srch=src->h-srcy)<1) return 0; }

  int srccolstride=akgl_fmt_get_pixel_size(src->fmt);
  if (srccolstride<1) return -1;
  int srcrowstride=srccolstride*src->w;

  uint32_t pixel=akgl_pixel_from_rgba(rgba);

  //TODO blit with scaling
  if (srcw>dstw) srcw=dstw;
  else if (srcw<dstw) dstw=srcw;
  if (srch>dsth) srch=dsth;
  else if (srch<dsth) dsth=srch;

  uint32_t *dstrow=dst->pixels+dsty*dst->w+dstx;
  uint8_t *srcrow=(uint8_t*)src->pixels+srcy*srcrowstride+srcx*srccolstride;
  while (dsth-->0) {
    uint8_t *srcp=srcrow;
    uint32_t *dstp=dstrow;
    dstrow+=dst->w;
    srcrow+=srcrowstride;
    int i=dstw; for (;i-->0;srcp+=srccolstride,dstp++) {
      uint8_t srca=srcp[0]; //TODO source pixel alpha
      if (srca) { //TODO blend
        *dstp=pixel;
      }
    }
  }

  return 0;
}
