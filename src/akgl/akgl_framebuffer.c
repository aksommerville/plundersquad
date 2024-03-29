#include "akgl_internal.h"

#if PS_NO_OPENGL2

struct akgl_framebuffer *akgl_framebuffer_new() {
  return (struct akgl_framebuffer*)ps_sdraw_image_new();
}

void akgl_framebuffer_del(struct akgl_framebuffer *framebuffer) {
  ps_sdraw_image_del((struct ps_sdraw_image*)framebuffer);
}

int akgl_framebuffer_ref(struct akgl_framebuffer *framebuffer) {
  return ps_sdraw_image_ref((struct ps_sdraw_image*)framebuffer);
}

int akgl_framebuffer_resize(struct akgl_framebuffer *framebuffer,int w,int h) {
  return ps_sdraw_image_realloc((struct ps_sdraw_image*)framebuffer,PS_SDRAW_FMT_RGBX,w,h);
}

int akgl_framebuffer_use(struct akgl_framebuffer *framebuffer) {
  if (framebuffer==akgl.framebuffer) return 0;
  if (framebuffer&&(akgl_framebuffer_ref(framebuffer)<0)) return -1;
  akgl_framebuffer_del(akgl.framebuffer);
  akgl.framebuffer=framebuffer;
  return 0;
}

#else

/* Object lifecycle.
 */

struct akgl_framebuffer *akgl_framebuffer_new() {
  if (!akgl.init) return 0;

  switch (akgl.strategy) {
    case AKGL_STRATEGY_SOFT: return (struct akgl_framebuffer*)ps_sdraw_image_new();
    case AKGL_STRATEGY_GL2: break;
    default: return 0;
  }
  
  struct akgl_framebuffer *framebuffer=calloc(1,sizeof(struct akgl_framebuffer));
  if (!framebuffer) return 0;

  framebuffer->refc=1;

  glGenTextures(1,&framebuffer->texid);
  if (akgl_get_error()) {
    ps_log(VIDEO,ERROR,"Failed to generate texture for framebuffer.");
    free(framebuffer);
    return 0;
  }
  
  glGenFramebuffers(1,&framebuffer->fbid);
  if (akgl_get_error()) {
    ps_log(VIDEO,ERROR,"Failed to create framebuffer.");
    glDeleteTextures(1,&framebuffer->texid);
    free(framebuffer);
    return 0;
  }

  glBindTexture(GL_TEXTURE_2D,framebuffer->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer->fbid);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,framebuffer->texid,0);
  
  if (akgl_clear_error()) {
    ps_log(VIDEO,ERROR,"Failed to create framebuffer.");
    akgl_framebuffer_del(framebuffer);
    return 0;
  }
  return framebuffer;
}

void akgl_framebuffer_del(struct akgl_framebuffer *framebuffer) {
  if (!framebuffer) return;
  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    ps_sdraw_image_del((struct ps_sdraw_image*)framebuffer);
    return;
  }
  if (framebuffer->refc-->1) return;

  glDeleteFramebuffers(1,&framebuffer->fbid);
  glDeleteTextures(1,&framebuffer->texid);

  free(framebuffer);
}

int akgl_framebuffer_ref(struct akgl_framebuffer *framebuffer) {
  if (!framebuffer) return -1;
  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    return ps_sdraw_image_ref((struct ps_sdraw_image*)framebuffer);
  }
  if (framebuffer->refc<1) return -1;
  if (framebuffer->refc==INT_MAX) return -1;
  framebuffer->refc++;
  return 0;
}

/* Resize.
 */

int akgl_framebuffer_resize(struct akgl_framebuffer *framebuffer,int w,int h) {
  if (!framebuffer) return -1;
  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    return ps_sdraw_image_realloc((struct ps_sdraw_image*)framebuffer,PS_SDRAW_FMT_RGBX,w,h);
  }
  if ((w==framebuffer->w)&&(h==framebuffer->h)) return 0;
  if ((w<1)||(h<1)) return -1;

  glBindTexture(GL_TEXTURE_2D,framebuffer->texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,0);

  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer->fbid);
  int status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status!=GL_FRAMEBUFFER_COMPLETE) return -1;

  if (akgl_clear_error()) {
    ps_log(VIDEO,ERROR,"Failed to resize framebuffer.");
    return -1;
  }
  framebuffer->w=w;
  framebuffer->h=h;
  return 0;
}

/* Enable framebuffer.
 */

int akgl_framebuffer_use(struct akgl_framebuffer *framebuffer) {
  if (framebuffer==akgl.framebuffer) return 0;
  
  if (akgl.strategy==AKGL_STRATEGY_GL2) {
    if (framebuffer) {
      glBindFramebuffer(GL_FRAMEBUFFER,framebuffer->fbid);
      //glViewport(-8,-8,framebuffer->w+8,framebuffer->h+8);
      glViewport(0,0,framebuffer->w,framebuffer->h);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER,0);
      glViewport(0,0,akgl.screenw,akgl.screenh);
    }
    if (akgl_clear_error()) {
      ps_log(VIDEO,ERROR,"Failed to enter framebuffer %p",framebuffer);
      return -1;
    }
  }
  
  if (framebuffer&&(akgl_framebuffer_ref(framebuffer)<0)) return -1;
  akgl_framebuffer_del(akgl.framebuffer);
  akgl.framebuffer=framebuffer;
  return 0;
}

#endif
