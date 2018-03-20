#include "ps.h"
#include "ps_sdraw.h"

/* Plain rectangle.
 */
 
int ps_sdraw_draw_rect(struct ps_sdraw_image *image,int x,int y,int w,int h,struct ps_sdraw_rgba rgba) {
  if (!image) return -1;
  if (!rgba.a) return 0;
  
  if ((w<1)||(h<1)) return 0;
  if (x<0) { if ((w+=x)<1) return 0; x=0; }
  if (y<0) { if ((h+=y)<1) return 0; y=0; }
  if (x>image->w-w) { if ((w=image->w-x)<1) return 0; }
  if (y>image->h-h) { if ((h=image->h-y)<1) return 0; }

  uint8_t *dstrow=image->pixels+y*image->rowstride+x*image->colstride;

  if (image->fmt==PS_SDRAW_FMT_A) {
    while (h-->0) {
      memset(dstrow,rgba.a,w);
      dstrow+=image->rowstride;
    }
  
  } else if (rgba.a==0xff) {
    switch (image->fmt) {
    
      case PS_SDRAW_FMT_RGB: { // RGB, no blending
          while (h-->0) {
            uint8_t *dst=dstrow;
            dstrow+=image->rowstride;
            int i=w; for (;i-->0;dst+=3) {
              dst[0]=rgba.r;
              dst[1]=rgba.g;
              dst[2]=rgba.b;
            }
          }
        } break;
    
      case PS_SDRAW_FMT_RGBX: 
      case PS_SDRAW_FMT_RGBA: { // RGBX or RGBA, no blending
          while (h-->0) {
            uint8_t *dst=dstrow;
            dstrow+=image->rowstride;
            int i=w; for (;i-->0;dst+=4) {
              dst[0]=rgba.r;
              dst[1]=rgba.g;
              dst[2]=rgba.b;
              dst[3]=0xff;
            }
          }
        } break;
    
      default: return -1;
    }

  } else {
    uint8_t dsta=0xff-rgba.a;
    int pmr=rgba.r*rgba.a;
    int pmg=rgba.g*rgba.a;
    int pmb=rgba.b*rgba.a;
    switch (image->fmt) {
    
      case PS_SDRAW_FMT_RGB: { // RGB, blending
          while (h-->0) {
            uint8_t *dst=dstrow;
            dstrow+=image->rowstride;
            int i=w; for (;i-->0;dst+=3) {
              dst[0]=(dst[0]*dsta+pmr)>>8;
              dst[1]=(dst[1]*dsta+pmg)>>8;
              dst[2]=(dst[2]*dsta+pmb)>>8;
            }
          }
        } break;
    
      case PS_SDRAW_FMT_RGBX: { // RGBX, blending
          while (h-->0) {
            uint8_t *dst=dstrow;
            dstrow+=image->rowstride;
            int i=w; for (;i-->0;dst+=4) {
              dst[0]=(dst[0]*dsta+pmr)>>8;
              dst[1]=(dst[1]*dsta+pmg)>>8;
              dst[2]=(dst[2]*dsta+pmb)>>8;
              dst[3]=0xff;
            }
          }
        } break;
        
      case PS_SDRAW_FMT_RGBA: { // RGBA, blending
          while (h-->0) {
            uint8_t *dst=dstrow;
            dstrow+=image->rowstride;
            int i=w; for (;i-->0;dst+=4) {
              if (!dst[3]) {
                dst[0]=rgba.r;
                dst[1]=rgba.g;
                dst[2]=rgba.b;
                dst[3]=rgba.a;
              } else if (dst[3]==0xff) {
                dst[0]=(dst[0]*dsta+pmr)>>8;
                dst[1]=(dst[1]*dsta+pmg)>>8;
                dst[2]=(dst[2]*dsta+pmb)>>8;
              } else {
                // I think this is incorrect; we should use alpha sum as the denominator.
                // Since it won't come up very often, this more efficient strategy is probably acceptable.
                uint8_t effdsta=(dsta<dst[3])?dsta:dst[3];
                dst[0]=(dst[0]*effdsta+pmr)>>8;
                dst[1]=(dst[1]*effdsta+pmg)>>8;
                dst[2]=(dst[2]*effdsta+pmb)>>8;
                dst[3]=effdsta+rgba.a;
              }
            }
          }
        } break;
    
      default: return -1;
    }
  }
  
  return 0;
}
