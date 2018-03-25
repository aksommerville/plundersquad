#include "akgl_internal.h"

struct akgl akgl={0};

/* Get error.
 */
 
int akgl_get_error() {
  int err=glGetError();
  if (err) {
    ps_log(VIDEO,ERROR,"GL error 0x%04x",err);
  }
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

int akgl_init(int strategy) {
  if (akgl.init) return -1;
  memset(&akgl,0,sizeof(struct akgl));
  akgl.init=1;

  switch (strategy) {
  
    case AKGL_STRATEGY_SOFT: {
        glGenTextures(1,&akgl.soft_fbtexid);
        if (!akgl.soft_fbtexid) {
          glGenTextures(1,&akgl.soft_fbtexid);
          if (!akgl.soft_fbtexid) {
            akgl.init=0;
            return -1;
          }
        }
        glBindTexture(GL_TEXTURE_2D,akgl.soft_fbtexid);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      } break;
        
    case AKGL_STRATEGY_GL2: {
        #if PS_NO_OPENGL2
          akgl.init=0;
          return -1;
        #endif
      } break;
      
    default: akgl.init=0; return -1;
  }
  akgl.strategy=strategy;

  akgl.screenw=640;
  akgl.screenh=480;
  akgl.glsl_version=120;

  #ifdef GL_POINT_SPRITE
    glEnable(GL_POINT_SPRITE);
  #endif

  return 0;
}

/* Quit.
 */

void akgl_quit() {
  akgl_framebuffer_del(akgl.framebuffer);
  if (akgl.soft_fbtexid) glDeleteTextures(1,&akgl.soft_fbtexid);
  memset(&akgl,0,sizeof(struct akgl));
}

/* Strategy.
 */

int akgl_get_strategy() {
  return akgl.strategy;
}

struct ps_sdraw_image *akgl_get_output_image() {
  if (akgl.strategy!=AKGL_STRATEGY_SOFT) return 0;
  return (struct ps_sdraw_image*)akgl.framebuffer;
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

#if PS_NO_OPENGL2
void akgl_set_uniform_screen_size(GLuint location) {
}
#else
void akgl_set_uniform_screen_size(GLuint location) {
  if (akgl.framebuffer) {
    glUniform2f(location,akgl.framebuffer->w,akgl.framebuffer->h);
  } else {
    glUniform2f(location,akgl.screenw,akgl.screenh);
  }
}
#endif

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

/* Set scissor.
 */

int akgl_scissor(int x,int y,int w,int h) {
  if (!akgl.init) return -1;
  
  //TODO In SOFT strategy, we will need support from ps_sdraw.
  if (akgl.strategy==AKGL_STRATEGY_SOFT) return 0;
  
  glEnable(GL_SCISSOR_TEST);
  if (akgl.framebuffer) {
    switch (akgl.strategy) {
      case AKGL_STRATEGY_SOFT: y=((struct ps_sdraw_image*)akgl.framebuffer)->h-y-h; break;
      case AKGL_STRATEGY_GL2: y=akgl.framebuffer->h-y-h; break;
    }
  } else {
    y=akgl.screenh-y-h;
  }
  glScissor(x,y,w,h);
  return 0;
}

int akgl_scissor_none() {
  if (!akgl.init) return 0;
  glDisable(GL_SCISSOR_TEST);
  return 0;
}
