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
