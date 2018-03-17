#ifndef AKGL_INTERNAL_H
#define AKGL_INTERNAL_H

#include "ps.h"
#include "akgl.h"
#include "soft/akgl_soft.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#if PS_ARCH==PS_ARCH_macos
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#elif PS_ARCH==PS_ARCH_raspi
  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>
#else
  #define GL_GLEXT_PROTOTYPES 1
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif

#ifndef GL_PROGRAM_POINT_SIZE
  #ifdef GL_PROGRAM_POINT_SIZE_EXT
    #define GL_PROGRAM_POINT_SIZE GL_PROGRAM_POINT_SIZE_EXT
  #elif defined(GL_VERTEX_PROGRAM_POINT_SIZE)
    #define GL_PROGRAM_POINT_SIZE GL_VERTEX_PROGRAM_POINT_SIZE
  #endif
#endif

#define AKGL_MAGIC_FRAMEBUFFER_SOFT   1
#define AKGL_MAGIC_FRAMEBUFFER_GL2    2
#define AKGL_MAGIC_PROGRAM_SOFT       3
#define AKGL_MAGIC_PROGRAM_GL2        4
#define AKGL_MAGIC_TEXTURE            5

struct akgl_texture {
  int refc;
  int magic;
  GLuint texid;
  int w,h;
  int fmt;
  int filter;
  struct akgl_framebuffer *parent;
};

struct akgl_framebuffer {
  int refc;
  int magic;
  GLuint fbid;
  GLuint texid;
  int w,h;
  int fmt;
  int filter;
};

struct akgl_program {
  int refc;
  int magic;
  GLuint programid;
  GLuint location_screensize;
  GLuint location_tilesize;
  char *error_log;
  int error_logc;
};

struct akgl_framebuffer_soft {
  int refc;
  int magic;
  int w,h;
  uint32_t *pixels;
  GLuint texid;
};

struct akgl_program_soft {
  int refc;
  int magic;
};

extern struct akgl {
  int init;
  int strategy;
  int glsl_version;
  int screenw,screenh;
  struct akgl_framebuffer *framebuffer;
} akgl;

void akgl_set_uniform_screen_size(GLuint location);

#endif
