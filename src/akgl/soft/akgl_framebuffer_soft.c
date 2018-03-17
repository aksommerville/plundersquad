#include "akgl/akgl_internal.h"

#define AKGL_FRAMEBUFFER_SOFT_SIZE_LIMIT 8192
#define AKGL_FRAMEBUFFER_SOFT_SIZE_LIMIT 8192

/* Object lifecycle.
 */

struct akgl_framebuffer_soft *akgl_framebuffer_soft_new() {
  struct akgl_framebuffer_soft *fb=calloc(1,sizeof(struct akgl_framebuffer_soft));
  if (!fb) return 0;

  fb->refc=1;
  fb->magic=AKGL_MAGIC_FRAMEBUFFER_SOFT;

  glGenTextures(1,&fb->texid);
  if (!fb->texid) {
    glGenTextures(1,&fb->texid);
    if (!fb->texid) {
      akgl_framebuffer_soft_del(fb);
      return 0;
    }
  }

  glBindTexture(GL_TEXTURE_2D,fb->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D,0);

  if (akgl_clear_error()) {
    akgl_framebuffer_soft_del(fb);
    return 0;
  }

  return fb;
}

void akgl_framebuffer_soft_del(struct akgl_framebuffer_soft *fb) {
  if (!fb) return;
  if (fb->refc-->1) return;

  if (fb->pixels) free(fb->pixels);
  if (fb->texid) glDeleteTextures(1,&fb->texid);

  free(fb);
}

int akgl_framebuffer_soft_ref(struct akgl_framebuffer_soft *fb) {
  if (!fb) return -1;
  if (fb->refc<1) return -1;
  if (fb->refc==INT_MAX) return -1;
  fb->refc++;
  return 0;
}

/* Resize.
 */
 
int akgl_framebuffer_soft_resize(struct akgl_framebuffer_soft *fb,int w,int h) {
  if (!fb) return -1;
  if (fb->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
  if ((w<1)||(w>AKGL_FRAMEBUFFER_SOFT_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>AKGL_FRAMEBUFFER_SOFT_SIZE_LIMIT)) return -1;
  if ((w==fb->w)&&(h==fb->h)) return 0;

  void *nv=calloc(w*h,4);
  if (!nv) return -1;
  if (fb->pixels) free(fb->pixels);
  fb->pixels=nv;
  fb->w=w;
  fb->h=h;

  return 0;
}

/* Use.
 */
 
int akgl_framebuffer_soft_use(struct akgl_framebuffer_soft *fb) {
  if (fb) {
    if (fb->magic!=AKGL_MAGIC_FRAMEBUFFER_SOFT) return -1;
    if (fb==(struct akgl_framebuffer_soft*)akgl.framebuffer) return 0;
    if (akgl_framebuffer_soft_ref(fb)<0) return -1;
    akgl_framebuffer_del(akgl.framebuffer);
    akgl.framebuffer=(struct akgl_framebuffer*)fb;
  } else {
    akgl_framebuffer_del(akgl.framebuffer);
    akgl.framebuffer=0;
  }
  return 0;
}
