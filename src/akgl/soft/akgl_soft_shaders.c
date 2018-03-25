#include "akgl/akgl_internal.h"

/* fbxfer
 */
 
int akgl_soft_fbxfer_draw(struct ps_sdraw_image *src,int x,int y,int w,int h) {
  if (!src) return -1;
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return -1;
  switch (src->fmt) {
    case PS_SDRAW_FMT_RGBX:
    case PS_SDRAW_FMT_RGBA:
      break;
    default: return -1;
  }

  glViewport(0,0,akgl.screenw,akgl.screenh);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,akgl.soft_fbtexid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,src->w,src->h,0,GL_RGBA,GL_UNSIGNED_BYTE,src->pixels);

  float dstl=(x*2.0f)/akgl.screenw-1.0f;
  float dstr=((x+w)*2.0f)/akgl.screenw-1.0f;
  float dstb=(y*2.0f)/akgl.screenh-1.0f;
  float dstt=((y+h)*2.0f)/akgl.screenh-1.0f;

  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2i(0,0); glVertex2f(dstl,dstt);
    glTexCoord2i(0,1); glVertex2f(dstl,dstb);
    glTexCoord2i(1,0); glVertex2f(dstr,dstt);
    glTexCoord2i(1,1); glVertex2f(dstr,dstb);
  glEnd();

  if (akgl_clear_error()) {
    return -1;
  }
  
  return 0;
}

/* maxtile
 */
 
int akgl_soft_maxtile_draw(struct akgl_texture *texture,const struct akgl_vtx_maxtile *vtxv,int vtxc) {
  if (vtxc<1) return 0;
  if (!texture||!vtxv) return -1;
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return -1;
  if (!akgl.framebuffer) return -1;
  struct ps_sdraw_image *src=(struct ps_sdraw_image*)texture;
  struct ps_sdraw_image *dst=(struct ps_sdraw_image*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;

  for (;vtxc--;vtxv++) {
    if (ps_sdraw_blit_maxtile(dst,vtxv,src)<0) return -1;
  }

  return 0;
}

/* mintile
 */

int akgl_soft_mintile_draw(struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size) {
  if (vtxc<1) return 0;
  if (size<1) return 0;
  if (!texture||!vtxv) return -1;
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return -1;
  if (!akgl.framebuffer) return -1;
  struct ps_sdraw_image *src=(struct ps_sdraw_image*)texture;
  struct ps_sdraw_image *dst=(struct ps_sdraw_image*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;
  int halfsize=size>>1;

  for (;vtxc-->0;vtxv++) {
    int dstx=vtxv->x-halfsize;
    int dsty=vtxv->y-halfsize;
    int srcx=(vtxv->tileid&0x0f)*srccolw;
    int srcy=(vtxv->tileid>>4)*srcrowh;
    if (ps_sdraw_blit(dst,dstx,dsty,size,size,src,srcx,srcy,srccolw,srcrowh)<0) return -1;
  }

  return 0;
}

/* Draw line.
 */

int akgl_soft_raw_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc,int width) {
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return -1;
  if (!akgl.framebuffer) return -1;
  struct ps_sdraw_image *dst=(struct ps_sdraw_image*)akgl.framebuffer;
  
  if (width!=1) {
    ps_log(VIDEO,ERROR,"Line width %d not supported.",width);
    return -1;
  }
  
  int i=1; for (;i<vtxc;i++) {
    //TODO If we want to keep the contract strictly, we'd need a gradient line function from sdraw. Doable.
    if (ps_sdraw_draw_line(dst,vtxv[i-1].x,vtxv[i-1].y,vtxv[i].x,vtxv[i].y,ps_sdraw_rgba(vtxv[i].r,vtxv[i].g,vtxv[i].b,vtxv[i].a))<0) return -1;
  }
  
  return 0;
}

/* Stubs XXX
 */

int akgl_soft_raw_draw_triangle_strip(const struct akgl_vtx_raw *vtxv,int vtxc) {
  ps_log(VIDEO,ERROR,"Stubbed function %s, didn't expect it to be called.",__func__);
  return -1;
}

int akgl_soft_raw_draw_points(const struct akgl_vtx_raw *vtxv,int vtxc,int size) {
  ps_log(VIDEO,ERROR,"Stubbed function %s, didn't expect it to be called.",__func__);
  return -1;
}

/* tex
 */

int akgl_soft_tex_draw(struct akgl_texture *tex,const struct akgl_vtx_tex *vtxv,int vtxc) {
  //TODO tex
  return 0;
}

/* textile
 */

int akgl_soft_textile_draw(struct akgl_texture *tex,const struct akgl_vtx_textile *vtxv,int vtxc) {
  if (vtxc<1) return 0;
  if (!tex||!vtxv) return -1;
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return -1;
  if (!akgl.framebuffer) return -1;
  struct ps_sdraw_image *src=(struct ps_sdraw_image*)tex;
  struct ps_sdraw_image *dst=(struct ps_sdraw_image*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;

  for (;vtxc-->0;vtxv++) {
    int dsth=vtxv->size;
    int dstw=dsth>>1;
    int dstx=vtxv->x-(dstw>>1);
    int dsty=vtxv->y-(dsth>>1);
    int srcx=srccolw*(vtxv->tileid&0x0f);
    int srcy=srcrowh*(vtxv->tileid>>4);
    if (ps_sdraw_blit_replacergb(
      dst,dstx,dsty,dstw,dsth,src,srcx,srcy,srccolw,srcrowh,ps_sdraw_rgba(vtxv->r,vtxv->g,vtxv->b,vtxv->a)
    )<0) return -1;
  }
  return 0;
}
