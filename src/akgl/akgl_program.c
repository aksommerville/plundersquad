#include "akgl_internal.h"
#include <stdio.h>

#if PS_NO_OPENGL2

struct akgl_program *akgl_program_new() { return 0; }
void akgl_program_del(struct akgl_program *program) { }
int akgl_program_ref(struct akgl_program *program) { return -1; }

int akgl_program_compile(
  struct akgl_program *program,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc
) { return -1; }

int akgl_program_handoff_error_log(struct akgl_program *program,char *log,int logc) { return -1; }
int akgl_program_get_error_log(void *dstpp,const struct akgl_program *program) { return -1; }
int akgl_program_use(struct akgl_program *program) { return -1; }

#else

/* Object lifecycle.
 */

struct akgl_program *akgl_program_new() {
  if (!akgl.init) return 0;
  
  if (akgl.strategy==AKGL_STRATEGY_SOFT) return 0;
  
  struct akgl_program *program=calloc(1,sizeof(struct akgl_program));
  if (!program) return 0;

  program->refc=1;
  if (!(program->programid=glCreateProgram())) {
    if (!(program->programid=glCreateProgram())) {
      free(program);
      return 0;
    }
  }

  return program;
}

void akgl_program_del(struct akgl_program *program) {
  if (!program) return;
  if (program->refc-->1) return;

  glDeleteProgram(program->programid);
  if (program->error_log) free(program->error_log);

  free(program);
}

int akgl_program_ref(struct akgl_program *program) {
  if (!program) return -1;
  if (program->refc<1) return -1;
  if (program->refc==INT_MAX) return -1;
  program->refc++;
  return 0;
}

/* Compile single shader.
 */

static GLuint akgl_program_compile_1(struct akgl_program *program,const char *src,int srcc,GLuint type) {

  char version[256];
  int versionc;
  versionc=snprintf(version,sizeof(version),"#version %d\n",akgl.glsl_version);
  if ((versionc<1)||(versionc>=sizeof(version))) return 0;
  const char *srcv[2]={version,src};
  GLint lenv[2]={versionc,srcc};

  GLuint id=glCreateShader(type);
  if (!id) return 0;
  glShaderSource(id,2,srcv,lenv);
  glCompileShader(id);

  GLint status=0;
  glGetShaderiv(id,GL_COMPILE_STATUS,&status);
  if (status) return id;

  GLint loga=0;
  glGetShaderiv(id,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(id,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        akgl_program_handoff_error_log(program,log,logc);
      } else {
        free(log);
      }
    }
  }
  glDeleteShader(id);
  return 0;
}

/* Link.
 */

static int akgl_program_link(struct akgl_program *program) {

  glLinkProgram(program->programid);
  GLint status=0;
  glGetProgramiv(program->programid,GL_LINK_STATUS,&status);
  if (status) return 0;

  /* Link failed. */
  GLint loga=0;
  glGetProgramiv(program->programid,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(program->programid,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        akgl_program_handoff_error_log(program,log,logc);
      } else {
        free(log);
      }
    }
  }
  return -1;
}

/* Bind attributes automatically from text.
 */
 
static int akgl_program_bind_attributes(struct akgl_program *program,const char *src,int srcc) {
  char zname[256];
  int attrid=0;
  int srcp=0; while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (srcp>srcc-9) break;
    if (memcmp(src+srcp,"attribute",9)) {
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) srcp++;
    } else {
      srcp+=9;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // type
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      const char *name=src+srcp;
      int namec=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!=';')) { srcp++; namec++; }
      if (!namec) return -1;
      if (namec>=sizeof(zname)) return -1;
      memcpy(zname,name,namec);
      zname[namec]=0;
      glBindAttribLocation(program->programid,attrid,zname);
      attrid++;
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) srcp++;
    }
  }
  return 0;
}

/* Compile, main entry point.
 */
 
int akgl_program_compile(
  struct akgl_program *program,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc
) {
  if (!program) return -1;
  if (!vsrc) vsrcc=0; else if (vsrcc<0) { vsrcc=0; while (vsrc[vsrcc]) vsrcc++; }
  if (!fsrc) fsrcc=0; else if (fsrcc<0) { fsrcc=0; while (fsrc[fsrcc]) fsrcc++; }

  GLuint vshader=akgl_program_compile_1(program,vsrc,vsrcc,GL_VERTEX_SHADER);
  if (!vshader) return -1;
  GLuint fshader=akgl_program_compile_1(program,fsrc,fsrcc,GL_FRAGMENT_SHADER);
  if (!fshader) {
    glDeleteShader(vshader);
    return -1;
  }

  glAttachShader(program->programid,vshader);
  glAttachShader(program->programid,fshader);
  glDeleteShader(vshader);
  glDeleteShader(fshader);

  if (akgl_program_bind_attributes(program,vsrc,vsrcc)<0) {
    return -1;
  }

  if (akgl_program_link(program)<0) {
    return -1;
  }
  
  program->location_screensize=glGetUniformLocation(program->programid,"screensize");
  program->location_tilesize=glGetUniformLocation(program->programid,"tilesize");

  return 0;
}

/* Error log.
 */
 
int akgl_program_handoff_error_log(struct akgl_program *program,char *log,int logc) {
  if (!program) return -1;
  if (log==program->error_log) return 0;
  if (!log) logc=0; else if (logc<0) { logc=0; if (log) while (log[logc]) logc++; }
  if (program->error_log) free(program->error_log);
  program->error_log=log;
  program->error_logc=logc;
  return 0;
}

int akgl_program_get_error_log(void *dstpp,const struct akgl_program *program) {
  if (!program) return 0;
  if (dstpp) *(void**)dstpp=program->error_log;
  return program->error_logc;
}

/* Enable program.
 */

int akgl_program_use(struct akgl_program *program) {
  if (program) {
    glUseProgram(program->programid);
    akgl_set_uniform_screen_size(program->location_screensize);
  } else {
    glUseProgram(0);
  }
  if (akgl_clear_error()) return -1;
  return 0;
}

#endif
