#include "akgl_internal.h"

#if PS_NO_OPENGL2

struct akgl_program *akgl_program_mintile_new() { return 0; }

int akgl_program_mintile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size) {
  return akgl_soft_mintile_draw(texture,vtxv,vtxc,size);
}

#else

/* GLSL.
 */

static const char akgl_vsrc_mintile[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 position;\n"
  "attribute float tile;\n"
  "varying vec2 vtexoffset;\n"
  "void main() {\n"
    "vec2 adjposition=(position*2.0)/screensize-1.0;\n"
    "adjposition.y=-adjposition.y;\n"
    "gl_Position=vec4(adjposition,0.0,1.0);\n"
    "vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/16.0;\n"
    "gl_PointSize=16.0;\n"
  "}\n"
"";

static const char akgl_fsrc_mintile[]=
  "uniform vec2 screensize;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexoffset;\n"
  "void main() {\n"
    "vec2 texcoord=gl_PointCoord;\n"
    "texcoord=texcoord/16.0+vtexoffset;\n"
    "gl_FragColor=texture2D(sampler,texcoord);\n"
  "}\n"
"";

/* Create shader.
 */

struct akgl_program *akgl_program_mintile_new() {
  struct akgl_program *program=akgl_program_new();
  if (!program) return 0;

  if (akgl_program_compile(
    program,
    akgl_vsrc_mintile,sizeof(akgl_vsrc_mintile)-1,
    akgl_fsrc_mintile,sizeof(akgl_fsrc_mintile)-1
  )<0) {
    const char *log=0;
    int logc=akgl_program_get_error_log(&log,program);
    if (log&&(logc>0)) {
      ps_log(VIDEO,ERROR,"Failed to compile 'mintile' shader...\n%.*s\n",logc,log);
    }
    akgl_program_del(program);
    return 0;
  }

  return program;
}

/* Draw.
 */
 
int akgl_program_mintile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size) {

  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    return akgl_soft_mintile_draw(texture,vtxv,vtxc,size);
  }

  if (!program||!texture) return -1;
  if (vtxc<1) return 0;
  if (!vtxv) return -1;
  if (size<1) return 0;

  //ps_log(VIDEO,DEBUG,"%s texid=%d vtxc=%d",__func__,texture->texid,vtxc);

  if (akgl_program_use(program)<0) return -1;
  #if PS_ARCH!=PS_ARCH_raspi
    glEnable(GL_TEXTURE_2D);
  #endif
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  #ifdef GL_PROGRAM_POINT_SIZE
    glDisable(GL_PROGRAM_POINT_SIZE);
  #endif
  #if PS_ARCH!=PS_ARCH_raspi
    glPointSize(size);
  #endif

  if (akgl_clear_error()) {
    ps_log(VIDEO,ERROR,"Failed to start mintile render.");
    return -1;
  }

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct akgl_vtx_mintile),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_mintile),&vtxv[0].tileid);
  glDrawArrays(GL_POINTS,0,vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  akgl.cmdc++;

  return 0;
}

#endif
