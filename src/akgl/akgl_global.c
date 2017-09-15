#include "akgl_internal.h"

struct akgl akgl={0};

/* Get error.
 */
 
int akgl_get_error() {
  int err=glGetError();
  return err;
}

int akgl_clear_error() {
  int err=glGetError();
  if (err) {
    int sanity=100;
    while (glGetError()&&(sanity-->0)) ;
  }
  return err;
}

/* Init.
 */

int akgl_init() {
  if (akgl.init) return -1;
  memset(&akgl,0,sizeof(struct akgl));
  akgl.init=1;

  akgl.screenw=640;
  akgl.screenh=480;
  akgl.glsl_version=120;

  glEnable(GL_POINT_SPRITE);

  return 0;
}

/* Quit.
 */

void akgl_quit() {
  akgl_framebuffer_del(akgl.framebuffer);
  memset(&akgl,0,sizeof(struct akgl));
}

/* Screen size.
 */
 
int akgl_set_screen_size(int w,int h) {
  if ((w<1)||(h<1)) return -1;
  akgl.screenw=w;
  akgl.screenh=h; 
  return 0;
}

int akgl_get_screen_size(int *w,int *h) {
  if (!akgl.init) return -1;
  if (w) *w=akgl.screenw;
  if (h) *h=akgl.screenh;
  return 0;
}

void akgl_set_uniform_screen_size(GLuint location) {
  if (akgl.framebuffer) {
    glUniform2f(location,akgl.framebuffer->w,akgl.framebuffer->h);
  } else {
    glUniform2f(location,akgl.screenw,akgl.screenh);
  }
}

/* GLSL version.
 */

int akgl_set_glsl_version(int version) {
  if (!akgl.init) return -1;
  if (version<100) return -1;
  akgl.glsl_version=version;
  return 0;
}

/* Clear current framebuffer.
 */
 
int akgl_clear(uint32_t rgba) {
  if (!akgl.init) return -1;
  glClearColor(
    (rgba>>24)/255.0f,
    ((rgba>>16)&0xff)/255.0f,
    ((rgba>>8)&0xff)/255.0f,
    (rgba&0xff)/255.0f
  );
  glClear(GL_COLOR_BUFFER_BIT);
  return 0;
}
