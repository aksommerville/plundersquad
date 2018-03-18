#include "akgl_internal.h"

/* Object lifecycle.
 */

struct akgl_texture *akgl_texture_new() {
  if (!akgl.init) return 0;

  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    return (struct akgl_texture*)akgl_texture_soft_new();
  }
  
  struct akgl_texture *texture=calloc(1,sizeof(struct akgl_texture));
  if (!texture) return 0;

  texture->refc=1;
  texture->magic=AKGL_MAGIC_TEXTURE_GL2;
  glGenTextures(1,&texture->texid);
  if (akgl_clear_error()) {
    free(texture);
    return 0;
  }

  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  if (akgl_clear_error()) {
    akgl_texture_del(texture);
    return 0;
  }

  return texture;
}

struct akgl_texture *akgl_texture_from_framebuffer(struct akgl_framebuffer *framebuffer) {
  if (!framebuffer) return 0;

  if (framebuffer->magic==AKGL_MAGIC_FRAMEBUFFER_SOFT) {
    return (struct akgl_texture*)akgl_texture_soft_from_framebuffer((struct akgl_framebuffer_soft*)framebuffer);
  }
  
  struct akgl_texture *texture=calloc(1,sizeof(struct akgl_texture));
  if (!texture) return 0;

  if (akgl_framebuffer_ref(framebuffer)<0) {
    free(texture);
    return 0;
  }
  texture->refc=1;
  texture->parent=framebuffer;
  texture->texid=framebuffer->texid;

  return texture;
}

void akgl_texture_del(struct akgl_texture *texture) {
  if (!texture) return;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) {
    akgl_texture_soft_del((struct akgl_texture_soft*)texture);
    return;
  }
  if (texture->refc-->1) return;

  if (texture->parent) {
    akgl_framebuffer_del(texture->parent);
  } else {
    glDeleteTextures(1,&texture->texid);
  }

  free(texture);
}

int akgl_texture_ref(struct akgl_texture *texture) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_ref((struct akgl_texture_soft*)texture);
  if (texture->refc<1) return -1;
  if (texture->refc==INT_MAX) return -1;
  texture->refc++;
  return 0;
}

/* Trivial accessors.
 */

int akgl_texture_get_fmt(const struct akgl_texture *texture) {
  if (!texture) return 0;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_get_fmt((struct akgl_texture_soft*)texture);
  if (texture->parent) return texture->parent->fmt;
  return texture->fmt;
}

int akgl_texture_get_size(int *w,int *h,const struct akgl_texture *texture) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_get_size(w,h,(struct akgl_texture_soft*)texture);
  if (w) *w=texture->parent?texture->parent->w:texture->w;
  if (h) *h=texture->parent?texture->parent->h:texture->h;
  return 0;
}

int akgl_texture_get_filter(const struct akgl_texture *texture) {
  if (!texture) return 0;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return 0;
  if (texture->parent) return texture->parent->filter;
  return texture->filter;
}

/* Set filter.
 */

int akgl_texture_set_filter(struct akgl_texture *texture,int filter) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return 0;
  filter=filter?1:0;
  if (texture->parent) {
    if (texture->parent->filter==filter) return 0;
  } else {
    if (texture->filter==filter) return 0;
  }
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  if (filter) {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
  int err=akgl_clear_error();
  if (err) return -1;
  if (texture->parent) {
    texture->parent->filter=filter;
  } else {
    texture->filter=filter;
  }
  return 0;
}

/* Load image data.
 */

int akgl_texture_load(
  struct akgl_texture *texture,
  const void *pixels,
  int fmt,int w,int h
) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_load((struct akgl_texture_soft*)texture,pixels,fmt,w,h);
  if (texture->parent) return -1;
  if (!pixels) return -1;
  if ((w<1)||(h<1)) return -1;
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  switch (fmt) {
    case AKGL_FMT_RGB8: glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_RGBA8: glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_Y8: glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,w,h,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_YA8: glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE_ALPHA,w,h,0,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_A8: glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,w,h,0,GL_ALPHA,GL_UNSIGNED_BYTE,pixels); break;
    default: return -1;
  }
  int err=akgl_clear_error();
  if (err) return -1;
  texture->fmt=fmt;
  texture->w=w;
  texture->h=h;
  return 0;
}

int akgl_texture_realloc(
  struct akgl_texture *texture,
  int fmt,int w,int h
) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_realloc((struct akgl_texture_soft*)texture,fmt,w,h);
  if (texture->parent) return -1;
  if ((w<1)||(h<1)) return -1;
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  switch (fmt) {
    case AKGL_FMT_RGB8: glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,0); break;
    case AKGL_FMT_RGBA8: glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,0); break;
    case AKGL_FMT_Y8: glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,w,h,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,0); break;
    case AKGL_FMT_YA8: glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE_ALPHA,w,h,0,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,0); break;
    case AKGL_FMT_A8: glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,w,h,0,GL_ALPHA,GL_UNSIGNED_BYTE,0); break;
    default: return -1;
  }
  int err=akgl_clear_error();
  if (err) return -1;
  texture->fmt=fmt;
  texture->w=w;
  texture->h=h;
  return 0;
}

int akgl_texture_load_sub(
  struct akgl_texture *texture,
  const void *pixels,
  int x,int y,int w,int h
) {
  if (!texture) return -1;
  if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return akgl_texture_soft_load_sub((struct akgl_texture_soft*)texture,pixels,x,y,w,h);
  if (texture->parent) return -1;
  if (!pixels) return -1;
  if ((w<1)||(h<1)) return -1;
  if ((x<0)||(y<0)) return -1;
  if (x>texture->w-w) return -1;
  if (y>texture->h-h) return -1;
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  switch (texture->fmt) {
    case AKGL_FMT_RGB8: glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_RGBA8: glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_Y8: glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_LUMINANCE,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_YA8: glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,pixels); break;
    case AKGL_FMT_A8: glTexSubImage2D(GL_TEXTURE_2D,0,x,y,w,h,GL_ALPHA,GL_UNSIGNED_BYTE,pixels); break;
    default: return -1;
  }
  int err=akgl_clear_error();
  if (err) return -1;
  return 0;
}

/* Enable texture.
 */

int akgl_texture_use(struct akgl_texture *texture) {
  if (texture) {
    if (texture->magic==AKGL_MAGIC_TEXTURE_SOFT) return 0;
    #if PS_ARCH!=PS_ARCH_raspi
      glEnable(GL_TEXTURE_2D);
    #endif
    glBindTexture(GL_TEXTURE_2D,texture->texid);
  } else {
    #if PS_ARCH!=PS_ARCH_raspi
      glDisable(GL_TEXTURE_2D);
    #endif
    glBindTexture(GL_TEXTURE_2D,0);
  }
  if (akgl_clear_error()) return -1;
  return 0;
}
