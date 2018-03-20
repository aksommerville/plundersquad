#include "ps.h"
#include "ps_sdraw.h"

#define PS_SDRAW_IMAGE_SIZE_LIMIT 2048

/* Object lifecycle.
 */
 
struct ps_sdraw_image *ps_sdraw_image_new() {
  struct ps_sdraw_image *image=calloc(1,sizeof(struct ps_sdraw_image));
  if (!image) return 0;

  image->refc=1;

  return image;
}

void ps_sdraw_image_del(struct ps_sdraw_image *image) {
  if (!image) return;
  if (image->refc-->0) return;
  if (image->pixels) free(image->pixels);
  free(image);
}

int ps_sdraw_image_ref(struct ps_sdraw_image *image) {
  if (!image) return -1;
  if (image->refc<1) return -1;
  if (image->refc==INT_MAX) return -1;
  image->refc++;
  return 0;
}

/* Reallocate.
 */

int ps_sdraw_image_realloc(struct ps_sdraw_image *image,int fmt,int w,int h) {
  if (!image) return -1;
  if ((w<1)||(w>PS_SDRAW_IMAGE_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>PS_SDRAW_IMAGE_SIZE_LIMIT)) return -1;

  // We promise to clear pixels, even if nothing changed:
  if ((fmt==image->fmt)&&(w==image->w)&&(h==image->h)) {
    memset(image->pixels,0,image->rowstride*image->h);
    return 0;
  }

  int colstride=ps_sdraw_fmt_pixel_size(fmt);
  if (colstride<1) return -1;
  int rowstride=colstride*w;
  void *nv=calloc(rowstride,h);
  if (!nv) return -1;
  
  if (image->pixels) free(image->pixels);
  image->pixels=nv;
  image->fmt=fmt;
  image->w=w;
  image->h=h;
  image->colstride=colstride;
  image->rowstride=rowstride;
  
  return 0;
}

/* Reallocate and load.
 */
 
int ps_sdraw_image_load(struct ps_sdraw_image *image,const void *pixels,int fmt,int w,int h) {
  if (!image) return -1;
  if ((image->fmt!=fmt)||(image->w!=w)||(image->h!=h)) {
    if (ps_sdraw_image_realloc(image,fmt,w,h)<0) return -1;
  }
  if (ps_sdraw_image_load_sub(image,pixels,0,0,w,h)<0) return -1;
  return 0;
}

/* Copy verbatim into image.
 */

int ps_sdraw_image_load_sub(struct ps_sdraw_image *image,const void *pixels,int x,int y,int w,int h) {
  if (!image) return -1;
  if ((x<0)||(y<0)) return -1;
  if ((w<1)||(h<1)) return -1;
  if ((x>image->w-w)||(y>image->h-h)) return -1;

  if (w==image->w) {
    // Full width, we can do a single memcpy.
    uint8_t *dst=image->pixels+y*image->rowstride;
    int cpc=image->rowstride*h;
    memcpy(dst,pixels,cpc);

  } else {
    // Copy row by row.
    int srcstride=image->colstride*w;
    const uint8_t *src=pixels;
    uint8_t *dst=image->pixels+image->rowstride*y+image->colstride*x;
    while (h-->0) {
      memcpy(dst,src,srcstride);
      src+=srcstride;
      dst+=image->rowstride;
    }
    
  }
  return 0;
}
