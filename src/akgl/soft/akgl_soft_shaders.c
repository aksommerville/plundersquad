#include "akgl/akgl_internal.h"

/* fbxfer
 */
 
int akgl_soft_fbxfer_draw(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h) {
  if (!fb) return -1;
  if (fb->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;

  glViewport(0,0,akgl.screenw,akgl.screenh);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,fb->texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,fb->w,fb->h,0,GL_RGBA,GL_UNSIGNED_BYTE,fb->pixels);

  double dstl=(x*2.0)/akgl.screenw-1.0;
  double dstr=((x+w)*2.0)/akgl.screenw-1.0;
  double dstb=(y*2.0)/akgl.screenh-1.0;
  double dstt=((y+h)*2.0)/akgl.screenh-1.0;

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
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  if (texture->magic!=AKGL_MAGIC_TEXTURE_SOFT) return -1;
  struct akgl_texture_soft *src=(struct akgl_texture_soft*)texture;
  struct akgl_framebuffer_soft *dst=(struct akgl_framebuffer_soft*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;

  for (;vtxc--;vtxv++) {
    int srcx=srccolw*(vtxv->tileid&0x0f);
    int srcy=srcrowh*(vtxv->tileid>>4);
    int dstx=vtxv->x-(vtxv->size>>1);
    int dsty=vtxv->y-(vtxv->size>>1);
    //TODO maxtile: primary, tint, rotation, xform
    if (akgl_framebuffer_soft_blit(dst,dstx,dsty,vtxv->size,vtxv->size,src,srcx,srcy,srccolw,srcrowh)<0) return -1;
  }

  return 0;
}

/* mintile
 */

int akgl_soft_mintile_draw(struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size) {
  if (vtxc<1) return 0;
  if (size<1) return 0;
  if (!texture||!vtxv) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  if (texture->magic!=AKGL_MAGIC_TEXTURE_SOFT) return -1;
  struct akgl_texture_soft *src=(struct akgl_texture_soft*)texture;
  struct akgl_framebuffer_soft *dst=(struct akgl_framebuffer_soft*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;
  int halfsize=size>>1;

  for (;vtxc-->0;vtxv++) {
    int dstx=vtxv->x-halfsize;
    int dsty=vtxv->y-halfsize;
    int srcx=(vtxv->tileid&0x0f)*srccolw;
    int srcy=(vtxv->tileid>>4)*srcrowh;
    if (akgl_framebuffer_soft_blit(dst,dstx,dsty,size,size,src,srcx,srcy,srccolw,srcrowh)<0) return -1;
  }
  
  return 0;
}

/* Glue to transform triangle strips into a rectangle, if possible.
 */

struct akgl_rectangle {
  int x,y,w,h;
  uint32_t nw,ne,sw,se;
};

static int akgl_rectangle_from_raw_vertices(struct akgl_rectangle *rectangle,const struct akgl_vtx_raw *vtxv,int vtxc) {
  if (vtxc!=4) return -1;

  int xa=vtxv[0].x;
  int ya=vtxv[0].y;
  int xz=vtxv[1].x;
  int yz=vtxv[1].y;
  if (xa>xz) { int tmp=xa; xa=xz; xz=tmp; }
  if (ya>ya) { int tmp=ya; ya=yz; yz=tmp; }
  int i; for (i=2;i<4;i++) {
    if (vtxv[i].x==xa) {
    } else if (vtxv[i].x==xz) {
    } else if (xa==xz) {
      if (vtxv[i].x>xa) xz=vtxv[i].x;
      else xa=vtxv[i].x;
    } else {
      return -1;
    }
    if (vtxv[i].y==ya) {
    } else if (vtxv[i].y==yz) {
    } else if (ya==yz) {
      if (vtxv[i].y>ya) yz=vtxv[i].y;
      else ya=vtxv[i].y;
    } else {
      return -1;
    }
  }

  rectangle->x=xa;
  rectangle->y=ya;
  rectangle->w=xz+1-xa;
  rectangle->h=yz+1-ya;
  for (i=0;i<4;i++) {
    uint32_t *dst;
    if (vtxv[i].x==xa) {
      if (vtxv[i].y==ya) dst=&rectangle->nw;
      else dst=&rectangle->sw;
    } else if (vtxv[i].y==ya) dst=&rectangle->ne;
    else dst=&rectangle->se;
    *dst=(vtxv[i].r<<24)|(vtxv[i].g<<16)|(vtxv[i].b<<8)|vtxv[i].a;
  }

  return 0;
}

/* Draw rectangle.
 */

static int akgl_draw_rectangle(const struct akgl_rectangle *rect) {
  if ((rect->nw==rect->ne)&&(rect->sw==rect->se)) {
    if (rect->nw==rect->sw) {
      return akgl_soft_draw_rect((struct akgl_framebuffer_soft*)akgl.framebuffer,rect->x,rect->y,rect->w,rect->h,rect->nw);
    } else {
      return akgl_soft_draw_vert_gradient((struct akgl_framebuffer_soft*)akgl.framebuffer,rect->x,rect->y,rect->w,rect->h,rect->nw,rect->sw);
    }
  } else if ((rect->nw==rect->sw)&&(rect->ne==rect->se)) {
    return akgl_soft_draw_horz_gradient((struct akgl_framebuffer_soft*)akgl.framebuffer,rect->x,rect->y,rect->w,rect->h,rect->nw,rect->ne);
  } else {
    return akgl_soft_draw_2d_gradient((struct akgl_framebuffer_soft*)akgl.framebuffer,rect->x,rect->y,rect->w,rect->h,rect->nw,rect->ne,rect->sw,rect->se);
  }
}

/* raw
 */

int akgl_soft_raw_draw_triangle_strip(const struct akgl_vtx_raw *vtxv,int vtxc) {
  if (vtxc<1) return 0;
  if (!vtxv) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;

  /* Usually a request to draw triangle strip is actually a rectangle.
   * Check for that and handle it special, because it's much easier than triangles.
   */
  struct akgl_rectangle rectangle;
  if (akgl_rectangle_from_raw_vertices(&rectangle,vtxv,vtxc)>=0) {
    return akgl_draw_rectangle(&rectangle);
  }
  
  //TODO triangle strip
  return 0;
}

int akgl_soft_raw_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc,int width) {
  if (vtxc<1) return 0;
  if (width<1) return 0;
  if (!vtxv) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  //TODO line strip
  return 0;
}

int akgl_soft_raw_draw_points(const struct akgl_vtx_raw *vtxv,int vtxc,int size) {
  if (vtxc<1) return 0;
  if (size<1) return 0;
  if (!vtxv) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  //TODO points
  return 0;
}

/* tex
 */

int akgl_soft_tex_draw(struct akgl_texture *tex,const struct akgl_vtx_tex *vtxv,int vtxc) {
  if (vtxc<1) return 0;
  if (!tex||!vtxv) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  //TODO tex
  return 0;
}

/* textile
 */

int akgl_soft_textile_draw(struct akgl_texture *tex,const struct akgl_vtx_textile *vtxv,int vtxc) {
  if (vtxc<1) return 0;
  if (!tex||!vtxv) return -1;
  if (tex->magic!=AKGL_MAGIC_TEXTURE_SOFT) return -1;
  if (!akgl.framebuffer) return -1;
  if (akgl.framebuffer->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  struct akgl_texture_soft *src=(struct akgl_texture_soft*)tex;
  struct akgl_framebuffer_soft *dst=(struct akgl_framebuffer_soft*)akgl.framebuffer;

  int srccolw=src->w>>4;
  int srcrowh=src->h>>4;

  for (;vtxc-->0;vtxv++) {
    int dsth=vtxv->size;
    int dstw=dsth>>1;
    int dstx=vtxv->x-(dstw>>1);
    int dsty=vtxv->y-(dsth>>1);
    int srcx=srccolw*(vtxv->tileid&0x0f);
    int srcy=srcrowh*(vtxv->tileid>>4);
    uint32_t rgba=(vtxv->r<<24)|(vtxv->g<<16)|(vtxv->b<<8)|vtxv->a;
    if (akgl_framebuffer_soft_blit_replacergb(
      dst,dstx,dsty,dstw,dsth,src,srcx,srcy,srccolw,srcrowh,rgba
    )<0) return -1;
  }
  
  return 0;
}
