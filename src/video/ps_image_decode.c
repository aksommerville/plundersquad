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

  int expect_size=w*h;
  switch (fmt) {
    case AKGL_FMT_Y8: break;
    case AKGL_FMT_YA8: expect_size<<=1; break;
    case AKGL_FMT_RGB8: expect_size*=3; break;
    case AKGL_FMT_RGBA8: expect_size<<=2; break;
    default: return 0;
  }
  if (srcc!=16+expect_size) return 0;

  struct akgl_texture *texture=akgl_texture_new();
  if (!texture) return 0;
  if (akgl_texture_load(texture,src+16,fmt,w,h)<0) {
    akgl_texture_del(texture);
    return 0;
  }
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
