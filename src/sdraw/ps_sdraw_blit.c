#include "ps.h"
#include "ps_sdraw.h"
#include "akgl/akgl.h" /* For struct akgl_vtx_maxtile */
#include <math.h>

/* Plain blit with scaling.
 */

static int ps_sdraw_blit__scale(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch
) {
  if ((dstw<1)||(dsth<1)||(srcw<1)||(srch<1)) return 0;
  
  //TODO Optimize plain blit with scaling.
  
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxrd||!pxwr) return -1;

  int dstxz=dstx+dstw;
  int dstyz=dsty+dsth;
  int dstyp=dsty; for (;dstyp<dstyz;dstyp++) {
    if (dstyp<0) continue;
    if (dstyp>=dst->h) break;
    int srcyp=srcy+((dstyp-dsty)*srch)/dsth;
    if (srcyp<0) continue;
    if (srcyp>=src->h) break;
    uint8_t *dstp=dst->pixels+dstyp*dst->rowstride+dstx*dst->colstride;
    const uint8_t *srcrow=src->pixels+srcyp*src->rowstride;

    int dstxp=dstx;; for (;dstxp<dstxz;dstxp++,dstp+=dst->colstride) {
      if (dstxp<0) continue;
      if (dstxp>=dst->w) break;
      int srcxp=srcx+((dstxp-dstx)*srcw)/dstw;
      if (srcxp<0) continue;
      if (srcxp>=src->w) break;

      pxwr(dstp,pxrd(srcrow+srcxp*src->colstride));
    }
  }

  return 0;
}

/* Plain blit.
 */
 
int ps_sdraw_blit(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch
) {
  if (!dst||!src) return -1;

  // It's a lot more complicated with scaling, so handle that separately.
  if ((dstw!=srcw)||(dsth!=srch)) {
    return ps_sdraw_blit__scale(dst,dstx,dsty,dstw,dsth,src,srcx,srcy,srcw,srch);
  }
  // From here on, we will ignore (dstw,dsth)...

  // Clip boundaries.
  if ((srcw<1)||(srch<1)) return 0;
  if (dstx<0) { if ((srcw+=dstx)<1) return 0; srcx-=dstx; dstx=0; }
  if (dsty<0) { if ((srch+=dsty)<1) return 0; srcy-=dsty; dsty=0; }
  if (srcx<0) { if ((srcw+=srcx)<1) return 0; dstx-=srcx; srcx=0; }
  if (srcy<0) { if ((srch+=srcy)<1) return 0; dsty-=srcy; srcy=0; }
  if (dstx>dst->w-srcw) { if ((srcw=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-srch) { if ((srch=dst->h-dsty)<1) return 0; }
  if (srcx>src->w-srcw) { if ((srcw=src->w-srcx)<1) return 0; }
  if (srcy>src->h-srch) { if ((srch=src->h-srcy)<1) return 0; }

  // Set up for iteration.
  const uint8_t *srcrow=src->pixels+srcy*src->rowstride+srcx*src->colstride;
  uint8_t *dstrow=dst->pixels+dsty*dst->rowstride+dstx*dst->colstride;

  // Can we do it with straight memcpy? Same formats, and no alpha.
  if ((dst->fmt==src->fmt)&&!ps_sdraw_fmt_has_alpha(src->fmt)) {
    int cpc=srcw*src->colstride;
    while (srch-->0) {
      memcpy(dstrow,srcrow,cpc);
      dstrow+=dst->rowstride;
      srcrow+=src->rowstride;
    }
    return 0;
  }

  //TODO More optimized format scenarios for plain blit.

  // General blit, one pixel at a time through generalized accessors.
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxrd||!pxwr) return -1;
  while (srch-->0) {
    uint8_t *dstp=dstrow; dstrow+=dst->rowstride;
    const uint8_t *srcp=srcrow; srcrow+=src->rowstride;
    int i=srcw; for (;i-->0;dstp+=dst->colstride,srcp+=src->colstride) {
      pxwr(dstp,pxrd(srcp));
    }
  }
  return 0;
}

/* Blit with scaling and replacement color.
 */

static int ps_sdraw_blit_replacergb__scale(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch,
  struct ps_sdraw_rgba rgba
) {
  if ((dstw<1)||(dsth<1)||(srcw<1)||(srch<1)) return 0;
  
  //TODO Optimize blit with scaling.
  
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxrd||!pxwr) return -1;

  int dstxz=dstx+dstw;
  int dstyz=dsty+dsth;
  int dstyp=dsty; for (;dstyp<dstyz;dstyp++) {
    if (dstyp<0) continue;
    if (dstyp>=dst->h) break;
    int srcyp=srcy+((dstyp-dsty)*srch)/dsth;
    if (srcyp<0) continue;
    if (srcyp>=src->h) break;
    uint8_t *dstp=dst->pixels+dstyp*dst->rowstride+dstx*dst->colstride;
    const uint8_t *srcrow=src->pixels+srcyp*src->rowstride;

    int dstxp=dstx;; for (;dstxp<dstxz;dstxp++,dstp+=dst->colstride) {
      if (dstxp<0) continue;
      if (dstxp>=dst->w) break;
      int srcxp=srcx+((dstxp-dstx)*srcw)/dstw;
      if (srcxp<0) continue;
      if (srcxp>=src->w) break;

      struct ps_sdraw_rgba pixel=pxrd(srcrow+srcxp*src->colstride);
      if (!pixel.a) continue;
      pixel.r=rgba.r;
      pixel.g=rgba.g;
      pixel.b=rgba.b;
      pxwr(dstp,pixel);
    }
  }

  return 0;
}

/* Blit with source alpha and replacement color.
 */
 
int ps_sdraw_blit_replacergb(
  struct ps_sdraw_image *dst,int dstx,int dsty,int dstw,int dsth,
  const struct ps_sdraw_image *src,int srcx,int srcy,int srcw,int srch,
  struct ps_sdraw_rgba rgba
) {
  if (!dst||!src) return -1;
  if (!rgba.a) return 0;

  /* Doing this from a no-alpha format doesn't make sense, but let's pick it off anyway.
   */
  if (!ps_sdraw_fmt_has_alpha(src->fmt)) {
    return ps_sdraw_draw_rect(dst,dstx,dsty,dstw,dsth,rgba);
  }

  /* Defer if scaling is needed.
   */
  if ((dstw!=srcw)||(dsth!=srch)) {
    return ps_sdraw_blit_replacergb__scale(dst,dstx,dsty,dstw,dsth,src,srcx,srcy,srcw,srch,rgba);
  }

  // Clip boundaries.
  if ((srcw<1)||(srch<1)) return 0;
  if (dstx<0) { if ((srcw+=dstx)<1) return 0; srcx-=dstx; dstx=0; }
  if (dsty<0) { if ((srch+=dsty)<1) return 0; srcy-=dsty; dsty=0; }
  if (srcx<0) { if ((srcw+=srcx)<1) return 0; dstx-=srcx; srcx=0; }
  if (srcy<0) { if ((srch+=srcy)<1) return 0; dsty-=srcy; srcy=0; }
  if (dstx>dst->w-srcw) { if ((srcw=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-srch) { if ((srch=dst->h-dsty)<1) return 0; }
  if (srcx>src->w-srcw) { if ((srcw=src->w-srcx)<1) return 0; }
  if (srcy>src->h-srch) { if ((srch=src->h-srcy)<1) return 0; }

  // Set up for iteration.
  const uint8_t *srcrow=src->pixels+srcy*src->rowstride+srcx*src->colstride;
  uint8_t *dstrow=dst->pixels+dsty*dst->rowstride+dstx*dst->colstride;

  //TODO More optimized format scenarios for color-replace blit.

  // Any output format, with the very likely input format of 'A'.
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxwr) return -1;
  if (src->fmt==PS_SDRAW_FMT_A) {
    uint8_t ka=rgba.a;
    while (srch-->0) {
      uint8_t *dstp=dstrow; dstrow+=dst->rowstride;
      const uint8_t *srcp=srcrow; srcrow+=src->rowstride;
      int i=srcw; for (;i-->0;dstp+=dst->colstride,srcp+=src->colstride) {
        rgba.a=((*srcp)*ka)>>8;
        pxwr(dstp,rgba);
      }
    }
    return 0;
  }

  // General blit, one pixel at a time through generalized accessors.
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  if (!pxrd) return -1;
  uint8_t ka=rgba.a;
  while (srch-->0) {
    uint8_t *dstp=dstrow; dstrow+=dst->rowstride;
    const uint8_t *srcp=srcrow; srcrow+=src->rowstride;
    int i=srcw; for (;i-->0;dstp+=dst->colstride,srcp+=src->colstride) {
      struct ps_sdraw_rgba pixel=pxrd(srcp);
      rgba.a=(pixel.a*ka)>>8;
      pxwr(dstp,rgba);
    }
  }

  return 0;
}

/* Bells-and-whistles blit, rotation or similar required.
 */
 
int ps_sdraw_blit_maxtile__rotate(
  struct ps_sdraw_image *dst,
  const struct akgl_vtx_maxtile *vtx,
  const struct ps_sdraw_image *src
) {

  /* Calculate source boundaries and center.
   */
  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;
  int srcl=(vtx->tileid&0x0f)*srccolw;
  int srct=(vtx->tileid>>4)*srcrowh;
  int srcr=srcl+srccolw;
  int srcb=srct+srcrowh;
  int srcx=(srcl+srcr)>>1;
  int srcy=(srct+srcb)>>1;

  /* Calculate the furthest pixel from destination center that we would need to draw.
   * This is ordinarily ceil(vtx->size/2), but with rotation it could be up to sqrt(2) of that.
   * To keep it simple and err on the side of overshooting, we'll extend by 1.5 if any rotation is present.
   */
  int radius=(vtx->size>>1)+1;
  if (vtx->t) radius=(3*radius)/2;

  /* Now calculate output bounds and clamp them to the image.
   * No need to clip source or preserve aspect or anything.
   */
  int dstl=vtx->x-radius; if (dstl<0) dstl=0;
  int dstt=vtx->y-radius; if (dstt<0) dstt=0;
  int dstr=vtx->x+radius; if (dstr>dst->w) dstr=dst->w;
  int dstb=vtx->y+radius; if (dstb>dst->h) dstb=dst->h;
  if ((dstl>=dstr)||(dstt>=dstb)) return 0;

  /* Produce an affine transform matrix describing rotation, scale, and xform from destination to source.
   * Each coordinate space is centered.
   */
  double mtx[4]={1.0,0.0,0.0,1.0};
  // Scale...
  if (vtx->size!=srccolw) mtx[0]=(mtx[0]*srccolw)/vtx->size;
  if (vtx->size!=srcrowh) mtx[3]=(mtx[3]*srcrowh)/vtx->size;
  // Axis-aligned transform...
  switch (vtx->xform) {
    case AKGL_XFORM_90: mtx[1]=mtx[3]; mtx[2]=-mtx[0]; mtx[3]=0.0; mtx[0]=0.0; break;
    case AKGL_XFORM_180: mtx[0]=-mtx[0]; mtx[3]=-mtx[3]; break;
    case AKGL_XFORM_270: mtx[1]=-mtx[3]; mtx[2]=mtx[0]; mtx[3]=0.0; mtx[0]=0.0; break;
    case AKGL_XFORM_FLOP: mtx[0]=-mtx[0]; break;
    case AKGL_XFORM_FLOP90: mtx[1]=mtx[3]; mtx[2]=mtx[0]; mtx[3]=0.0; mtx[0]=0.0; break;
    case AKGL_XFORM_FLOP180: mtx[3]=-mtx[3]; break;
    case AKGL_XFORM_FLOP270: mtx[1]=-mtx[3]; mtx[2]=-mtx[0]; mtx[3]=0.0; mtx[0]=0.0; break;
  }
  // Rotation...
  if (vtx->t) {
    double t=(vtx->t*M_PI*2.0)/256.0;
    double cost=cos(t);
    double sint=sin(t);
    double rot[4]={
       cost, sint,
      -sint, cost,
    };
    double prd[4]={
      mtx[0]*rot[0]+mtx[1]*rot[2],mtx[0]*rot[1]+mtx[1]*rot[3],
      mtx[2]*rot[0]+mtx[3]*rot[2],mtx[2]*rot[1]+mtx[3]*rot[3],
    };
    memcpy(mtx,prd,sizeof(prd));
  }
  
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxrd||!pxwr) return -1;

  /* Premultiply tint.
   */
  int trpm=vtx->tr*vtx->ta;
  int tgpm=vtx->tg*vtx->ta;
  int tbpm=vtx->tb*vtx->ta;

  /* Got our boundaries and our matrix and our accessors, vamanos!
   */
  int ox=dstl; for (;ox<dstr;ox++) {
    double fox=ox-vtx->x;
    int oy=dstt; for (;oy<dstb;oy++) {
      double foy=oy-vtx->y;
      
      int ix=srcx+lround(fox*mtx[0]+foy*mtx[1]);
      if ((ix<srcl)||(ix>=srcr)) continue;
      int iy=srcy+lround(fox*mtx[2]+foy*mtx[3]);
      if ((iy<srct)||(iy>=srcb)) continue;

      const uint8_t *srcp=src->pixels+iy*src->rowstride+ix*src->colstride;
      uint8_t *dstp=dst->pixels+oy*dst->rowstride+ox*dst->colstride;
      struct ps_sdraw_rgba pixel=pxrd(srcp);

      if (!pixel.a) continue;

      /* Primary color. */
      if (pixel.r&&(pixel.r!=0xff)) {
        if ((pixel.r==pixel.g)&&(pixel.g==pixel.b)) {
          int level=pixel.r;
          if (level<0x80) { // 0..t
            pixel.r=(vtx->pr*level)>>7;
            pixel.g=(vtx->pg*level)>>7;
            pixel.b=(vtx->pb*level)>>7;
          } else { // t..1
            level-=0x80;
            pixel.r=vtx->pr+(((255-vtx->pr)*level)>>7);
            pixel.g=vtx->pg+(((255-vtx->pg)*level)>>7);
            pixel.b=vtx->pb+(((255-vtx->pb)*level)>>7);
          }
        }
      }

      /* Tint. */
      if (vtx->ta) {
        int inv=0xff-vtx->ta;
        pixel.r=(pixel.r*inv+trpm)>>8;
        pixel.g=(pixel.g*inv+tgpm)>>8;
        pixel.b=(pixel.b*inv+tbpm)>>8;
      }

      /* Alpha. */
      if (vtx->a!=0xff) {
        pixel.a=(pixel.a*vtx->a)>>8;
        if (!pixel.a) continue;
      }

      pxwr(dstp,pixel);
    }
  }

  return 0;
}

/* Bells-and-whistles blit.
 */
 
int ps_sdraw_blit_maxtile(
  struct ps_sdraw_image *dst,
  const struct akgl_vtx_maxtile *vtx,
  const struct ps_sdraw_image *src
) {
  if (!dst||!vtx||!src) return -1;
  if (!vtx->a) return 0;
  if (vtx->size<1) return 0;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;

  /* First off, determine which fancy features are being requested.
   */
  int f_scale=((vtx->size!=srccolw)||(vtx->size!=srcrowh));
  int f_tint=vtx->ta;
  int f_primary=((vtx->pr!=0x80)||(vtx->pg!=0x80)||(vtx->pb!=0x80));
  int f_alpha=(vtx->a!=0xff);
  int f_rotate=vtx->t;
  int f_xform=(vtx->xform!=AKGL_XFORM_NONE);

  /* Calculate source and destination positions.
   * Destination position will be different later if we are rotating.
   * Even if that is the case, we cull from the unrotated bounds. Shouldn't matter much.
   */
  int dstx=vtx->x-(vtx->size>>1);
  int dsty=vtx->y-(vtx->size>>1);
  if (dstx>=dst->w) return 0;
  if (dsty>=dst->h) return 0;
  if (dstx+vtx->size<=0) return 0;
  if (dsty+vtx->size<=0) return 0;
  int srcx=(vtx->tileid&0x0f)*srccolw;
  int srcy=(vtx->tileid>>4)*srcrowh;

  /* It's more common than one might think for all the fancy stuff to be off.
   * If everything is off, or only (scale), we can use plain blit, which is likely to be a lot more efficient.
   */
  if (!f_tint&&!f_primary&&!f_alpha&&!f_rotate&&!f_xform) {
    return ps_sdraw_blit(
      dst,dstx,dsty,vtx->size,vtx->size,
      src,srcx,srcy,srccolw,srcrowh
    );
  }

  /* If rotation or scaling is enabled, we use a rather different approach.
   */
  if (f_scale||f_rotate) {
    return ps_sdraw_blit_maxtile__rotate(dst,vtx,src);
  }

  /* If there's a transform and the output will clip, it's awkward.
   * We'll detect that case and defer to __rotate since it is more tolerant of coordinate goofiness.
   * Note that this special case is *only* necessary when clipping.
   */
  if (f_xform) {
    if ((dstx<0)||(dsty<0)||(dstx>dst->w-srccolw)||(dsty>dst->h-srcrowh)) {
      return ps_sdraw_blit_maxtile__rotate(dst,vtx,src);
    }
  }

  /* No rotation or scaling; we are transferring 1:1.
   * It is now safe to clip boundaries.
   * Source boundaries are guaranteed in-bounds due to calculations above.
   */
  if (dstx<0) { if ((srccolw+=dstx)<1) return 0; srcx-=dstx; dstx=0; }
  if (dsty<0) { if ((srcrowh+=dsty)<1) return 0; srcy-=dsty; dsty=0; }
  if (dstx>dst->w-srccolw) { if ((srccolw=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-srcrowh) { if ((srcrowh=dst->h-dsty)<1) return 0; }

  /* Get sub-images and pixel accessors.
   */
  const uint8_t *srcrow=src->pixels+srcy*src->rowstride+srcx*src->colstride;
  uint8_t *dstrow=dst->pixels+dsty*dst->rowstride+dstx*dst->colstride;
  ps_sdraw_pxrd_fn pxrd=ps_sdraw_pxrd_for_fmt(src->fmt);
  ps_sdraw_pxwr_fn pxwr=ps_sdraw_pxwr_for_fmt(dst->fmt);
  if (!pxrd||!pxwr) return -1;

  /* Prepare source iterator based on xform.
   */
  int srcdx,srcdy;
  switch (vtx->xform) {
    case AKGL_XFORM_NONE: { // LRTB
        srcdx=src->colstride;
        srcdy=src->rowstride;
      } break;
    case AKGL_XFORM_90: { // BTLR
        srcrow+=src->rowstride*(srcrowh-1);
        srcdx=-src->rowstride;
        srcdy=src->colstride;
      } break;
    case AKGL_XFORM_180: { // RLBT
        srcrow+=src->rowstride*(srcrowh-1)+src->colstride*(srccolw-1);
        srcdx=-src->colstride;
        srcdy=-src->rowstride;
      } break;
    case AKGL_XFORM_270: { // TBRL
        srcrow+=src->colstride*(srccolw-1);
        srcdx=src->rowstride;
        srcdy=-src->colstride;
      } break;
    case AKGL_XFORM_FLOP: { // RLTB
        srcrow+=src->colstride*(srccolw-1);
        srcdx=-src->colstride;
        srcdy=src->rowstride;
      } break;
    case AKGL_XFORM_FLOP90: { // BTRL
        srcrow+=src->rowstride*(srcrowh-1)+src->colstride*(srccolw-1);
        srcdx=-src->rowstride;
        srcdy=-src->colstride;
      } break;
    case AKGL_XFORM_FLOP180: { // LRBT
        srcrow+=src->rowstride*(srcrowh-1);
        srcdx=src->colstride;
        srcdy=-src->rowstride;
      } break;
    case AKGL_XFORM_FLOP270: { // TBLR
        srcdx=src->rowstride;
        srcdy=src->colstride;
      } break;
    default: return -1;
  }

  /* Premultiply tint.
   */
  int trpm=vtx->tr*vtx->ta;
  int tgpm=vtx->tg*vtx->ta;
  int tbpm=vtx->tb*vtx->ta;

  /* We are now ready to iterate.
   * Within the pixel transfer, we need to examine three features: tint, primary, alpha.
   */
  int yp=0; for (;yp<srcrowh;yp++,dstrow+=dst->rowstride,srcrow+=srcdy) {
    uint8_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    int xp=0; for (;xp<srccolw;xp++,dstp+=dst->colstride,srcp+=srcdx) {
      struct ps_sdraw_rgba pixel=pxrd(srcp);
      if (!pixel.a) continue; // No amount of adjustment can make it opaque again.

      /* Primary color. */
      if (pixel.r&&(pixel.r!=0xff)) {
        if ((pixel.r==pixel.g)&&(pixel.g==pixel.b)) {
          int level=pixel.r;
          if (level<0x80) { // 0..t
            pixel.r=(vtx->pr*level)>>7;
            pixel.g=(vtx->pg*level)>>7;
            pixel.b=(vtx->pb*level)>>7;
          } else { // t..1
            level-=0x80;
            pixel.r=vtx->pr+(((255-vtx->pr)*level)>>7);
            pixel.g=vtx->pg+(((255-vtx->pg)*level)>>7);
            pixel.b=vtx->pb+(((255-vtx->pb)*level)>>7);
          }
        }
      }

      /* Tint. */
      if (vtx->ta) {
        int inv=0xff-vtx->ta;
        pixel.r=(pixel.r*inv+trpm)>>8;
        pixel.g=(pixel.g*inv+tgpm)>>8;
        pixel.b=(pixel.b*inv+tbpm)>>8;
      }

      /* Alpha. */
      if (f_alpha) {
        pixel.a=(pixel.a*vtx->a)>>8;
      }

      pxwr(dstp,pixel);
    }
  }

  return 0;
}
