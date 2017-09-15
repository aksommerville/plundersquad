#ifndef AKGL_INTERNAL_H
#define AKGL_INTERNAL_H

#include "akgl.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#if __APPLE__
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#else
  #include <gl/gl.h>
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
  int glsl_version;
  int screenw,screenh;
  struct akgl_framebuffer *framebuffer;
} akgl;

void akgl_set_uniform_screen_size(GLuint location);

#endif
