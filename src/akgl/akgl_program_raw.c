#include "akgl_internal.h"

/* GLSL.
 */
 
static const char akgl_vsrc_raw[]=
  "uniform vec2 screensize;\n"
  "varying vec4 vcolor;\n"
  "attribute vec2 position;\n"
  "attribute vec4 color;\n"
  "void main() {\n"
    "vec2 adjposition=(position*2.0)/screensize-1.0;\n"
    "adjposition.y=-adjposition.y;\n"
    "gl_Position=vec4(adjposition,0.0,1.0);\n"
    "vcolor=color;\n"
  "}\n"
"";

static const char akgl_fsrc_raw[]=
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "gl_FragColor=vcolor;\n"
  "}\n"
"";

/* Create program.
 */
 
struct akgl_program *akgl_program_raw_new() {
  struct akgl_program *program=akgl_program_new();
  if (!program) return 0;

  if (akgl_program_compile(
    program,
    akgl_vsrc_raw,sizeof(akgl_vsrc_raw)-1,
    akgl_fsrc_raw,sizeof(akgl_vsrc_raw)-1
  )<0) {
    const char *log=0;
    int logc=akgl_program_get_error_log(&log,program);
    if (log&&(logc>0)) {
      ps_log(VIDEO,ERROR,"Failed to compile 'raw' shader...\n%.*s\n",logc,log);
    }
    akgl_program_del(program);
    return 0;
  }

  return program;
}

/* Draw primitives.
 */

static int akgl_program_raw_draw_primitives(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc,int type) {
  if (!vtxv||(vtxc<1)) return -1;
  if (akgl_program_use(program)<0) return -1;
  if (akgl_texture_use(0)<0) return -1;
  
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct akgl_vtx_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct akgl_vtx_raw),&vtxv[0].r);
  glDrawArrays(type,0,vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  return 0;
}

/* Draw primitives, public hooks.
 */
 
int akgl_program_raw_draw_triangle_strip(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc) {
  return akgl_program_raw_draw_primitives(program,vtxv,vtxc,GL_TRIANGLE_STRIP);
}

int akgl_program_raw_draw_line_strip(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc,int width) {
  if (width<1) return -1;
  glLineWidth(width);
  return akgl_program_raw_draw_primitives(program,vtxv,vtxc,GL_LINE_STRIP);
}

int akgl_program_raw_draw_points(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc,int size) {
  if (size<1) return -1;
  #ifdef GL_PROGRAM_POINT_SIZE
    glDisable(GL_PROGRAM_POINT_SIZE);
  #endif
  #if PS_ARCH!=PS_ARCH_raspi
    glPointSize(size);
  #endif
  return akgl_program_raw_draw_primitives(program,vtxv,vtxc,GL_POINTS);
}
