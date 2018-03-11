#include "ps.h"
#include "ps_image_decode.h"
#include "akgl/akgl.h"
#include "akpng/akpng.h"

/* Decode PNG.
 */

static int ps_png_force_akgl_format(struct akpng_image *image) {

  /* All 4 AKGL formats are native PNG formats, in fact they are very likely ones. */
  if (image->depth==8) switch (image->colortype) {
    case 0: return AKGL_FMT_Y8;
    case 2: return AKGL_FMT_RGB8;
    case 4: return AKGL_FMT_YA8;
    case 6: return AKGL_FMT_RGBA8;
  }

  int color=((image->colortype!=0)&&(image->colortype!=4));
  int alpha=akpng_image_has_transparency(image);

  if (!color&&!alpha) {
    if (akpng_image_force_gray(image)<0) return -1;
    return AKGL_FMT_Y8;
  } else if (alpha) {
    if (akpng_image_force_rgb(image,1)<0) return -1;
    return AKGL_FMT_RGBA8;
  } else {
    if (akpng_image_force_rgb(image,-1)<0) return -1;
    return AKGL_FMT_RGB8;
  }
}

static struct akgl_texture *ps_texture_from_png(struct akpng_image *image,int fmt) {
  struct akgl_texture *texture=akgl_texture_new();
  if (!texture) return 0;

  if (akgl_texture_load(texture,image->pixels,fmt,image->w,image->h)<0) {
    akgl_texture_del(texture);
    return 0;
  }

  return texture;
}

static struct akgl_texture *ps_image_decode_png(const void *src,int srcc,const char *refpath) {
  struct akpng_image image={0};

  if (akpng_decode(&image,src,srcc)<0) {
    ps_log(RES,ERROR,"%s: Failed to decode PNG.",refpath);
    akpng_image_cleanup(&image);
    return 0;
  }

  int fmt=ps_png_force_akgl_format(&image);
  if (fmt<0) {
    ps_log(RES,ERROR,"%s: Unable to force PNG to acceptable format.",refpath);
    akpng_image_cleanup(&image);
    return 0;
  }

  struct akgl_texture *texture=ps_texture_from_png(&image,fmt);

  akpng_image_cleanup(&image);
  return texture;
}

/* Decode psimage.
 */

static struct akgl_texture *ps_image_decode_psimage(const uint8_t *src,int srcc,const char *refpath) {
  if (srcc<16) return 0;
  int w=(src[8]<<8)|src[9];
  int h=(src[10]<<8)|src[11];
  int fmt=src[12];
  if (!w||!h) return 0;
  if (w>(INT_MAX/h)>>2) return 0;

  int pixelsize=ps_pixelsize_for_akgl_fmt(fmt);
  if (pixelsize<1) return 0;
  int stride=w*pixelsize;
  if (stride>INT_MAX/h) return 0;
  int expect_size=stride*h;
  if (srcc!=16+expect_size) return 0;
  
  void *dst=malloc(expect_size);
  if (!dst) return 0;
  ps_image_unfilter(dst,src+16,stride,h);

  struct akgl_texture *texture=akgl_texture_new();
  if (akgl_texture_load(texture,dst,fmt,w,h)<0) {
    akgl_texture_del(texture);
    free(dst);
    return 0;
  }
  free(dst);
  return texture;
}

/* Decode raw pixels.
 */

static struct akgl_texture *ps_image_decode_raw(const void *src,int srcc,const char *refpath,int fmt) {
  struct akgl_texture *texture=akgl_texture_new();
  if (!texture) return 0;
  if (akgl_texture_load(texture,src,fmt,PS_TILESIZE*16,PS_TILESIZE*16)<0) {
    akgl_texture_del(texture);
    return 0;
  }
  return texture;
}

/* Decode image, main entry point.
 */
 
struct akgl_texture *ps_image_decode(
  const void *src,int srcc,
  const char *refpath
) {
  if (!src||(srcc<0)) return 0;
  if (!refpath) refpath="<unknown>";

  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) {
    return ps_image_decode_png(src,srcc,refpath);
  }

  if ((srcc>=8)&&!memcmp(src,"PSIMG\0\xff\0",8)) {
    return ps_image_decode_psimage(src,srcc,refpath);
  }

  const int tilesheet_size=(PS_TILESIZE*PS_TILESIZE)*256;
  if (srcc==tilesheet_size*1) return ps_image_decode_raw(src,srcc,refpath,AKGL_FMT_Y8);
  if (srcc==tilesheet_size*2) return ps_image_decode_raw(src,srcc,refpath,AKGL_FMT_YA8);
  if (srcc==tilesheet_size*3) return ps_image_decode_raw(src,srcc,refpath,AKGL_FMT_RGB8);
  if (srcc==tilesheet_size*4) return ps_image_decode_raw(src,srcc,refpath,AKGL_FMT_RGBA8);

  ps_log(RES,ERROR,"%s: Unable to detect image format.",refpath);
  return 0;
}

/* Decode image as raw pixels, main entry point.
 */
 
int ps_image_decode_pixels(
  void *pixelspp,int *fmt,int *w,int *h,
  const void *src,int srcc
) {
  if (!src) return -1;
  if (srcc<0) return -1;
  if (!pixelspp||!fmt||!w||!h) return -1;
  
  struct akpng_image image={0};

  if (akpng_decode(&image,src,srcc)<0) {
    akpng_image_cleanup(&image);
    return -1;
  }

  *fmt=ps_png_force_akgl_format(&image);
  if (*fmt<0) {
    akpng_image_cleanup(&image);
    return -1;
  }

  *(void**)pixelspp=image.pixels;
  image.pixels=0;
  *w=image.w;
  *h=image.h;

  akpng_image_cleanup(&image);
  return 0;
}

/* Encode image to psimage format.
 */
 
int ps_image_encode(void *dst,int dsta,const void *pixels,int w,int h,int fmt) {
  if (!dst||(dsta<0)) dsta=0;
  if (!pixels||(w<1)||(h<1)) return -1;
  if (w>0xffff) return -1;
  if (h>0xffff) return -1;
  
  int pixelsize=ps_pixelsize_for_akgl_fmt(fmt);
  if (pixelsize<1) return -1;
  if (w>INT_MAX/pixelsize) return -1;
  int stride=w*pixelsize;
  if (stride>INT_MAX/h) return -1;
  int bodysize=stride*h;
  if (bodysize>INT_MAX-16) return -1;
  int dstc=16+bodysize;
  
  if (dstc<=dsta) {
  
    uint8_t *DST=dst;
    memcpy(DST,"PSIMG\0\xff\0",8);
    DST[8]=w>>8;
    DST[9]=w;
    DST[10]=h>>8;
    DST[11]=h;
    DST[12]=fmt;
    DST[13]=0;
    DST[14]=0;
    DST[15]=0;
    
    ps_image_filter(DST+16,pixels,stride,h);
    
  }
  return dstc;
}

/* AKGL formats.
 */
 
int ps_pixelsize_for_akgl_fmt(int fmt) {
  switch (fmt) {
    case AKGL_FMT_RGBA8: return 4;
    case AKGL_FMT_RGB8: return 3;
    case AKGL_FMT_Y8: return 1;
    case AKGL_FMT_YA8: return 2;
    case AKGL_FMT_A8: return 1;
  }
  return 0;
}

/* Plain image filter.
 */
 
void ps_image_filter(void *dst,const void *src,int stride,int h) {
  if (!dst||!src) return;
  if ((stride<1)||(h<1)) return;
  
  /* TODO Image filter: Either improve it or take it out.
   * I thought for sure the PNG SUB filter would improve compressibility, turns out the opposite.
   * Maybe we'd do better with Paeth?
   * It's not a really big deal either way.
   *
   * 112497 with filter
   *  96736 without filter
   */
  memcpy(dst,src,stride*h);
  return;
  
  /* Copy the first row verbatim and start pointers at second row. */
  memcpy(dst,src,stride);
  const uint8_t *pv=src;
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  DST+=stride;
  SRC+=stride;
  h--;
  int pixelc=stride*h;
  
  /* Copy one pixel at a time. */
  while (pixelc-->0) {
    *DST=*SRC-*pv;
    DST++;
    SRC++;
    pv++;
  }
}

void ps_image_unfilter(void *dst,const void *src,int stride,int h) {
  if (!dst||!src) return;
  if ((stride<1)||(h<1)) return;
  
  memcpy(dst,src,stride*h);
  return;
  
  /* Copy the first row verbatim and start pointers at second row. */
  memcpy(dst,src,stride);
  const uint8_t *pv=dst;
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  DST+=stride;
  SRC+=stride;
  h--;
  int pixelc=stride*h;
  
  /* Copy one pixel at a time. */
  while (pixelc-->0) {
    *DST=*SRC+*pv;
    DST++;
    SRC++;
    pv++;
  }
}
