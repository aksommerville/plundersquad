#include "akpng_internal.h"

/* Clean up.
 */

void akpng_image_cleanup(struct akpng_image *image) {
  if (!image) return;
  if (image->pixels) free(image->pixels);
  if (image->chunkv) {
    while (image->chunkc-->0) {
      if (image->chunkv[image->chunkc].v) {
        free(image->chunkv[image->chunkc].v);
      }
    }
    free(image->chunkv);
  }
  memset(image,0,sizeof(struct akpng_image));
}

/* Reallocate image.
 */
 
int akpng_image_realloc(struct akpng_image *image,int w,int h,int depth,int colortype) {

  /* Validate arguments and calculate a few intermediaries. */
  if (!image) return -1;
  if (w<1) return -1;
  if (h<1) return -1;
  if (depth<1) return -1;
  if (depth>64) return -1;
  int chanc=akpng_chanc_for_colortype(colortype,depth);
  if (chanc<1) return -1;
  int pixelsize=depth*chanc;
  if (w>INT_MAX/pixelsize) return -1;
  int stride=(w*pixelsize+7)>>3;
  if (stride>INT_MAX/h) return -1;

  /* Allocate the new pixels. */
  void *pixels=calloc(stride,h);
  if (!pixels) return -1;

  /* Wipe image and rebuild it. */
  akpng_image_cleanup(image);
  image->pixels=pixels;
  image->w=w;
  image->h=h;
  image->depth=depth;
  image->colortype=colortype;
  image->stride=stride;

  return 0;
}

/* Replace one image with another, eg at the end of an "in-place" conversion that needed an intermediary.
 */
 
void akpng_image_handoff_content(struct akpng_image *dst,const struct akpng_image *src) {
  if (!dst||!src) return;

  // Free pixels.
  if (dst->pixels) free(dst->pixels);

  // Transfer pixels and header.
  dst->pixels=src->pixels;
  dst->w=src->w;
  dst->h=src->h;
  dst->depth=src->depth;
  dst->colortype=src->colortype;
  dst->stride=src->stride;

  // Remove PLTE and tRNS if present.
  int i=0;
  struct akpng_chunk *chunk=dst->chunkv;
  while (i<dst->chunkc) {
    if (!memcmp(chunk->type,"PLTE",4)||!memcmp(chunk->type,"tRNS",4)) {
      if (chunk->v) free(chunk->v);
      dst->chunkc--;
      memmove(chunk,chunk+1,sizeof(struct akpng_chunk)*(dst->chunkc-i));
    } else {
      i++;
      chunk++;
    }
  }
}

/* Add a chunk.
 */

int akpng_image_chunkv_require(struct akpng_image *image) {
  if (image->chunkc<image->chunka) return 0;
  int na=image->chunka+4;
  if (na>INT_MAX/sizeof(struct akpng_chunk)) return -1;
  void *nv=realloc(image->chunkv,sizeof(struct akpng_chunk)*na);
  if (!nv) return -1;
  image->chunkv=nv;
  image->chunka=na;
  return 0;
}
 
int akpng_image_copy_chunk(struct akpng_image *image,const struct akpng_chunk *chunk) {
  if (!chunk) return -1;
  return akpng_image_create_chunk(image,chunk->type,chunk->v,chunk->c);
}

int akpng_image_create_chunk(struct akpng_image *image,const char *type,const void *v,int c) {
  if ((c<0)||(c&&!v)) return -1;
  void *nv=0;
  if (c) {
    if (!(nv=malloc(c))) return -1;
    memcpy(nv,v,c);
  }
  if (akpng_image_create_chunk_handoff(image,type,nv,c)<0) {
    if (nv) free(nv);
    return -1;
  }
  return 0;
}
  
int akpng_image_create_chunk_handoff(struct akpng_image *image,const char *type,void *v,int c) {
  if (!image) return -1;
  if (!type) return -1;
  if ((c<0)||(c&&!v)) return -1;
  int i=0; for (;i<4;i++) {
    if ((type[i]<0x20)||(type[i]>0x7e)) return -1;
  }
  if (akpng_image_chunkv_require(image)<0) return -1;
  struct akpng_chunk *chunk=image->chunkv+image->chunkc++;
  memcpy(chunk->type,type,4);
  chunk->v=v;
  chunk->c=c;
  return 0;
}

/* Retrieve a chunk.
 */
 
struct akpng_chunk *akpng_image_get_chunk(const struct akpng_image *image,const char *type,int index) {
  if (!image) return 0;
  if (!type) return 0;
  if (index<0) return 0;
  struct akpng_chunk *chunk=image->chunkv;
  int i=0; for (;i<image->chunkc;i++,chunk++) {
    if (!memcmp(chunk->type,type,4)) {
      if (!index--) return chunk;
    }
  }
  return 0;
}

/* Test whether transparency is possible in an image.
 */

int akpng_image_has_transparency(const struct akpng_image *image) {
  if (!image) return 0;

  /* Has an alpha channel? */
  switch (image->colortype) {
    case 4: case 6: return 1;
  }

  /* No tRNS, or empty, no transparency. */
  const struct akpng_chunk *trns=akpng_image_get_chunk(image,"tRNS",0);
  if (!trns) return 0;
  if (trns->c<1) return 0;

  /* Confirm tRNS is valid for image format. */
  if (image->colortype==3) {
    return 1; // tRNS contains 1-byte alphas parallel to PLTE; short list permitted.
  }
  int chanc=akpng_chanc_for_colortype(image->colortype,image->depth);
  if (trns->c!=chanc*2) return 0;

  return 1;
}

/* Convert from 16-bit to 8-bit channels, regardless of colortype.
 * Dealing with depth of 8 or 16, rows are always packed.
 * So we can iterate over the entire image instead of row-by-row. Cool!
 */

static int akpng_image_convert_8_16(struct akpng_image *image) {
  if (!image) return -1;
  if (image->depth!=16) return -1;
  if ((image->w<1)||(image->h<1)) return -1;
  int dststride=image->stride>>1;
  if (dststride<1) return -1;
  int chanc=akpng_chanc_for_colortype(image->colortype,image->depth);
  if (chanc<1) return -1;
  if (chanc>INT_MAX/image->w) return -1;
  int fieldc=chanc*image->w;
  if (fieldc>INT_MAX/image->h) return -1;
  fieldc*=image->h;
  uint8_t *dstp=image->pixels;
  const uint8_t *srcp=image->pixels;
  while (fieldc-->0) {
    *dstp=*srcp;
    dstp++;
    srcp+=2;
  }
  image->depth=8;
  image->stride=dststride;
  return 0;
}

/* Convert to RGB from RGBA (source depth 8 or 16).
 */

static int akpng_image_convert_rgb8_rgba(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype!=6) return -1;
  if ((image->depth!=8)&&(image->depth!=16)) return -1;
  if ((image->w<1)||(image->h<1)) return -1;
  if (image->w>INT_MAX/image->h) return -1;
  int pixelc=image->w*image->h;
  uint8_t *dstp=image->pixels;
  const uint8_t *srcp=image->pixels;
  if (image->depth==8) {
    while (pixelc-->0) {
      dstp[0]=srcp[0];
      dstp[1]=srcp[1];
      dstp[2]=srcp[2];
      dstp+=3;
      srcp+=4;
    }
  } else if (image->depth==16) {
    while (pixelc-->0) {
      dstp[0]=srcp[0];
      dstp[1]=srcp[2];
      dstp[2]=srcp[4];
      dstp+=3;
      srcp+=8;
    }
  } else return -1;
  image->colortype=2;
  image->depth=8;
  image->stride=image->w*3;
  return 0;
}

/* To RGB8 from gray. Alpha may be present, and the actual colortype may be 3.
 */

static int akpng_image_convert_rgb8_gray(struct akpng_image *image) {
  if (!image) return -1;
  int alpha=(image->colortype==4);

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,2)<0) return -1;

  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,1)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    unsigned int luma=srci.read(&srci);
    if (alpha) srci.read(&srci); // Skip alpha if present.
    *dstp++=luma;
    *dstp++=luma;
    *dstp++=luma;
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To RGB8 from indexed (1,2,4,8 bit; pass on to 'gray' if no color table).
 */

static int akpng_image_convert_rgb8_index(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype!=3) return -1;

  // Missing or empty palette is a violation, but we'll just treat it as grayscale.
  const struct akpng_chunk *plte=akpng_image_get_chunk(image,"PLTE",0);
  if (!plte||(plte->c<3)) return akpng_image_convert_rgb8_gray(image);
  unsigned int pltec=plte->c/3; // Count of legal colors.

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,2)<0) return -1;

  /* Read from an iterator for simplicity's sake. We don't need an output iterator. */
  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,0)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    unsigned int index=srci.read(&srci);
    if (index>=pltec) {
      // Spec 11.2.3: Any pixel out of PLTE range is an error. We will tolerate this case by looping the palette.
      index%=pltec;
    }
    index*=3;
    *dstp++=((uint8_t*)plte->v)[index++];
    *dstp++=((uint8_t*)plte->v)[index++];
    *dstp++=((uint8_t*)plte->v)[index];
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To RGBA8 from RGB (8 or 16 bit).
 */

static int akpng_image_convert_rgba8_rgb(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype!=2) return -1;
  if ((image->depth!=8)&&(image->depth!=16)) return -1;

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,6)<0) return -1;

  /* Locate and validate tRNS. Ignore if invalid. */
  const struct akpng_chunk *trns=akpng_image_get_chunk(image,"tRNS",0);
  int trnsr,trnsg,trnsb;
  if (trns) {
    if (trns->c!=6) trns=0;
    trnsr=(((uint8_t*)trns->v)[0]<<8)|((uint8_t*)trns->v)[1];
    trnsg=(((uint8_t*)trns->v)[2]<<8)|((uint8_t*)trns->v)[3];
    trnsb=(((uint8_t*)trns->v)[4]<<8)|((uint8_t*)trns->v)[5];
  }

  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,0)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    if (trns) {
      int r=srci.read(&srci);
      int g=srci.read(&srci);
      int b=srci.read(&srci);
      if ((r==trnsr)&&(g==trnsg)&&(b==trnsb)) {
        *dstp++=0;
        *dstp++=0;
        *dstp++=0;
        *dstp++=0;
      } else if (image->depth==16) {
        *dstp++=r>>8;
        *dstp++=g>>8;
        *dstp++=b>>8;
        *dstp++=0xff;
      } else {
        *dstp++=r;
        *dstp++=g;
        *dstp++=b;
        *dstp++=0xff;
      }
    } else {
      if (image->depth==16) {
        *dstp++=srci.read(&srci)>>8;
        *dstp++=srci.read(&srci)>>8;
        *dstp++=srci.read(&srci)>>8;
      } else {
        *dstp++=srci.read(&srci);
        *dstp++=srci.read(&srci);
        *dstp++=srci.read(&srci);
      }
      *dstp++=0xff;
    }
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To RGBA8 from gray. Alpha may be present, and the actual colortype may be 3.
 */

static int akpng_image_convert_rgba8_gray(struct akpng_image *image) {
  if (!image) return -1;

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,6)<0) return -1;

  /* Plain grayscale supports tRNS. */
  int colorkey=-1;
  if (image->colortype==0) {
    const struct akpng_chunk *trns=akpng_image_get_chunk(image,"tRNS",0);
    if (trns&&(trns->c==2)) {
      colorkey=(((uint8_t*)trns->v)[0]<<8)|((uint8_t*)trns->v)[1];
    }
  }

  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,colorkey<0)<0) { // Normalize if colorkey absent.
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    unsigned int luma=srci.read(&srci);
    uint8_t alpha=0xff;
    if (colorkey>=0) {
      if (luma==colorkey) {
        luma=0;
        alpha=0;
      } else switch (image->depth) {
        case 1: luma=luma?0xff:0; break;
        case 2: luma|=luma<<2; luma|=luma<<4; break;
        case 4: luma|=luma<<4; break;
        case 16: luma>>=8; break;
      }
    } else if (image->colortype==4) {
      alpha=srci.read(&srci);
    }
    *dstp++=luma;
    *dstp++=luma;
    *dstp++=luma;
    *dstp++=alpha;
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To RGBA8 from indexed (1,2,4,8 bit; pass on to 'gray' if no color table).
 */

static int akpng_image_convert_rgba8_index(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype!=3) return -1;

  // Missing or empty palette is a violation, but we'll just treat it as grayscale.
  const struct akpng_chunk *plte=akpng_image_get_chunk(image,"PLTE",0);
  if (!plte||(plte->c<3)) return akpng_image_convert_rgba8_gray(image);
  unsigned int pltec=plte->c/3; // Count of legal colors.

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,6)<0) return -1;

  const struct akpng_chunk *trns=akpng_image_get_chunk(image,"tRNS",0);

  /* Read from an iterator for simplicity's sake. We don't need an output iterator. */
  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,0)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    unsigned int index=srci.read(&srci);
    uint8_t alpha=0xff;
    if (trns&&(index<trns->c)) alpha=((uint8_t*)trns->v)[index];
    if (index>=pltec) {
      // Spec 11.2.3: Any pixel out of PLTE range is an error. We will tolerate this case by looping the palette.
      index%=pltec;
    }
    index*=3;
    *dstp++=((uint8_t*)plte->v)[index++];
    *dstp++=((uint8_t*)plte->v)[index++];
    *dstp++=((uint8_t*)plte->v)[index];
    *dstp++=alpha;
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To gray from gray. Normalize to 8-bit and drop alpha.
 */

static int akpng_image_convert_gray8_gray(struct akpng_image *image) {
  if (!image) return -1;
  int alpha=(image->colortype==4);

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,0)<0) return -1;

  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,1)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    unsigned int luma=srci.read(&srci);
    if (alpha) srci.read(&srci); // Skip alpha if present.
    *dstp++=luma;
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To gray from RGB[A].
 */

static int akpng_image_convert_gray8_rgb(struct akpng_image *image) {
  if (!image) return -1;
  int alpha=(image->colortype==6);

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,0)<0) return -1;

  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,1)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    uint8_t r=srci.read(&srci);
    uint8_t g=srci.read(&srci);
    uint8_t b=srci.read(&srci);
    if (alpha) srci.read(&srci); // Skip alpha if present.
    *dstp++=(r+g+b)/3;
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* To gray from indexed (1,2,4,8 bit; pass on to 'gray' if no color table).
 */

static int akpng_image_convert_gray8_index(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype!=3) return -1;
  if (image->depth>8) return -1;

  // Missing or empty palette is a violation, but we'll just treat it as grayscale.
  const struct akpng_chunk *plte=akpng_image_get_chunk(image,"PLTE",0);
  if (!plte||(plte->c<3)) return akpng_image_convert_gray8_gray(image);
  unsigned int pltec=plte->c/3; // Count of legal colors.

  struct akpng_image dstimage={0};
  if (akpng_image_realloc(&dstimage,image->w,image->h,8,0)<0) return -1;

  /* It should be more efficient to convert the entire color table first. 
   * Anything out of range becomes black (technically a violation).
   */
  uint8_t ctab[256];
  const uint8_t *pltep=plte->v;
  int i; for (i=0;i<pltec;i++,pltep+=3) {
    if (i>=pltec) {
      memset(ctab+i,0,256-i);
      break;
    }
    ctab[i]=(pltep[0]+pltep[1]+pltep[2])/3;
  }

  /* Read from an iterator for simplicity's sake. We don't need an output iterator. */
  struct akpng_iterator srci={0};
  if (akpng_iterator_setup(&srci,image,0)<0) {
    akpng_image_cleanup(&dstimage);
    return -1;
  }
  uint8_t *dstp=dstimage.pixels;
  int pixelc=image->w*image->h;
  while (pixelc-->0) {
    *dstp++=ctab[srci.read(&srci)];
  }

  akpng_image_handoff_content(image,&dstimage);
  return 0;
}

/* Convert to RGB.
 */
 
int akpng_image_force_rgb(struct akpng_image *image,int alpha) {
  if (!image) return -1;
  if (!alpha) alpha=(akpng_image_has_transparency(image)?1:-1);
  if (alpha<0) { // To RGB...
    if (image->colortype==2) { // Already RGB...
      if (image->depth==8) return 0; // Redundant.
      if (image->depth!=16) return -1; // Illegal (RGB only 8 or 16).
      return akpng_image_convert_8_16(image);
    } else if (image->colortype==6) { // From RGBA, just strip alpha...
      return akpng_image_convert_rgb8_rgba(image);
    } else if (image->colortype==3) { // From indexed...
      return akpng_image_convert_rgb8_index(image);
    } else if ((image->colortype==0)||(image->colortype==4)) {
      return akpng_image_convert_rgb8_gray(image);
    }
  } else { // To RGBA...
    if (image->colortype==2) { // From RGB, just add alpha...
      return akpng_image_convert_rgba8_rgb(image);
    } else if (image->colortype==6) { // Already RGBA...
      if (image->depth==8) return 0; // Redundant.
      if (image->depth!=16) return -1; // Illegal (RGBA only 8 or 16).
      return akpng_image_convert_8_16(image);
    } else if ((image->colortype==0)||(image->colortype==4)) { // From gray...
      return akpng_image_convert_rgba8_gray(image);
    } else if (image->colortype==3) {
      return akpng_image_convert_rgba8_index(image);
    }
  }
  return -1;
}

/* Convert to grayscale.
 */
 
int akpng_image_force_gray(struct akpng_image *image) {
  if (!image) return -1;
  if (image->colortype==0) { // Already plain gray...
    if (image->depth==8) return 0; // Redundant.
    if (image->depth==16) return akpng_image_convert_8_16(image);
  }
  if ((image->colortype==0)||(image->colortype==4)) { // From gray, maybe alpha...
    return akpng_image_convert_gray8_gray(image);
  }
  if (image->colortype==3) { // From index...
    return akpng_image_convert_gray8_index(image);
  }
  return akpng_image_convert_gray8_rgb(image);
}

/* Iterator primitives.
 */

#define STEP(nshift,retval) \
  if (iterator->x>=iterator->w) { \
    if (iterator->y>=iterator->h-1) return retval; \
    iterator->x=0; \
    iterator->y++; \
    iterator->row+=iterator->stride; \
    iterator->p=iterator->row; \
    iterator->shift=nshift; \
  }

static int akpng_iterator_read_1(struct akpng_iterator *iterator) {
  STEP(7,0)
  int v=((iterator->p[0]>>iterator->shift)&1);
  if (iterator->normalize&&v) v=0xff;
  iterator->x++;
  if (!iterator->shift--) {
    iterator->shift=7;
    iterator->p++;
  }
  return v;
}

static void akpng_iterator_write_1(struct akpng_iterator *iterator,int v) {
  STEP(7,)
  if (iterator->normalize) v>>=7;
  if (v&1) {
    iterator->p[0]|=(1<<iterator->shift);
  } else {
    iterator->p[0]&=~(1<<iterator->shift);
  }
  iterator->x++;
  if (!iterator->shift--) {
    iterator->shift=7;
    iterator->p++;
  }
}

static int akpng_iterator_read_2(struct akpng_iterator *iterator) {
  STEP(6,0)
  int v=((iterator->p[0]>>iterator->shift)&3);
  if (iterator->normalize) switch (v) {
    case 1: v=0x55; break;
    case 2: v=0xaa; break;
    case 3: v=0xff; break;
  }
  iterator->x++;
  if ((iterator->shift-=2)<0) {
    iterator->shift=6;
    iterator->p++;
  }
  return v;
}

static void akpng_iterator_write_2(struct akpng_iterator *iterator,int v) {
  STEP(6,)
  if (iterator->normalize) v>>=6;
  uint8_t mask; switch (iterator->shift) {
    case 6: mask=0x3f; break;
    case 4: mask=0xcf; break;
    case 2: mask=0xf3; break;
    case 0: mask=0xfc; break;
  }
  iterator->p[0]=(iterator->p[0]&mask)|(v<<iterator->shift);
  iterator->x++;
  if ((iterator->shift-=2)<0) {
    iterator->shift=6;
    iterator->p++;
  }
}

static int akpng_iterator_read_4(struct akpng_iterator *iterator) {
  STEP(4,0)
  int v=((iterator->p[0]>>iterator->shift)&15);
  if (iterator->normalize) v|=v<<4;
  iterator->x++;
  if (iterator->shift) {
    iterator->shift=0;
  } else {
    iterator->shift=4;
    iterator->p++;
  }
  return v;
}

static void akpng_iterator_write_4(struct akpng_iterator *iterator,int v) {
  STEP(4,)
  if (iterator->normalize) v>>=4;
  if (iterator->shift) {
    iterator->p[0]=(iterator->p[0]&0x0f)|(v<<4);
  } else {
    iterator->p[0]=(iterator->p[0]&0xf0)|v;
  }
  iterator->x++;
  if (iterator->shift) {
    iterator->shift=0;
  } else {
    iterator->shift=4;
    iterator->p++;
  }
}

static int akpng_iterator_read_8(struct akpng_iterator *iterator) {
  STEP(0,0)
  int v=iterator->p[0];
  iterator->x++;
  iterator->p++;
  return v;
}

static void akpng_iterator_write_8(struct akpng_iterator *iterator,int v) {
  STEP(0,)
  iterator->p[0]=v;
  iterator->x++;
  iterator->p++;
}

static int akpng_iterator_read_16(struct akpng_iterator *iterator) {
  STEP(0,0)
  int v;
  if (iterator->normalize) {
    v=iterator->p[0];
  } else {
    v=(iterator->p[0]<<8)|iterator->p[1];
  }
  iterator->x++;
  iterator->p+=2;
  return v;
}

static void akpng_iterator_write_16(struct akpng_iterator *iterator,int v) {
  STEP(0,)
  iterator->p[0]=(iterator->normalize?v:(v>>8));
  iterator->p[1]=v;
  iterator->x++;
  iterator->p+=2;
}

#undef STEP

/* Channel iterator.
 */
 
int akpng_iterator_setup(struct akpng_iterator *iterator,const struct akpng_image *image,int normalize) {
  if (!iterator) return -1;
  if (!image) return -1;
  int chanc=akpng_chanc_for_colortype(image->colortype,image->depth);
  if (chanc<1) return -1;
  if ((image->w<1)||(image->h<1)) return -1;
  if (image->w>INT_MAX/chanc) return -1;
  switch (image->depth) {
    case 1: {
        iterator->read=akpng_iterator_read_1;
        iterator->write=akpng_iterator_write_1;
        iterator->shift=7;
      } break;
    case 2: {
        iterator->read=akpng_iterator_read_2;
        iterator->write=akpng_iterator_write_2;
        iterator->shift=6;
      } break;
    case 4: {
        iterator->read=akpng_iterator_read_4;
        iterator->write=akpng_iterator_write_4;
        iterator->shift=4;
      } break;
    case 8: {
        iterator->read=akpng_iterator_read_8;
        iterator->write=akpng_iterator_write_8;
      } break;
    case 16: {
        iterator->read=akpng_iterator_read_16;
        iterator->write=akpng_iterator_write_16;
      } break;
    default: return -1;
  }
  iterator->row=image->pixels;
  iterator->x=0;
  iterator->y=0;
  iterator->w=image->w*chanc;
  iterator->h=image->h;
  iterator->stride=image->stride;
  iterator->p=image->pixels;
  iterator->normalize=normalize;
  return 0;
}
