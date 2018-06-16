#ifndef AKGL_INTERNAL_H
#define AKGL_INTERNAL_H

#include "ps.h"
#include "akgl.h"
#include "soft/akgl_soft.h"
#include "sdraw/ps_sdraw.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#if PS_ARCH==PS_ARCH_macos
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#elif PS_ARCH==PS_ARCH_raspi
  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>
#elif PS_ARCH==PS_ARCH_mswin
  #define GLEW_STATIC 1
  #include <GL/glew.h>
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

struct akgl_texture {
  int refc;
  GLuint texid;
  int w,h;
  int fmt;
  int filter;
  struct akgl_framebuffer *parent;
};

struct akgl_framebuffer {
  int refc;
  GLuint fbid;
  GLuint texid;
  int w,h;
  int fmt;
  int filter;
};

struct akgl_program {
  int refc;
  GLuint programid;
  GLuint location_screensize;
  GLuint location_tilesize;
  char *error_log;
  int error_logc;
};

extern struct akgl {
  int init;
  int strategy;
  int glsl_version;
  int screenw,screenh;
  struct akgl_framebuffer *framebuffer;
  GLuint soft_fbtexid;
  int cmdc;
  int cmdc_logged;
} akgl;

void akgl_set_uniform_screen_size(GLuint location);

#endif
