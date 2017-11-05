#include "akgl_internal.h"

/* GLSL.
 */

static const char akgl_vsrc_textile[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 position;\n"
  "attribute float tile;\n"
  "attribute vec4 color;\n"
  "attribute float size;\n"
  "varying vec2 vtexoffset;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "vec2 adjposition=(position*2.0)/screensize-1.0;\n"
    "adjposition.y=-adjposition.y;\n"
    "gl_Position=vec4(adjposition,0.0,1.0);\n"
    "vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/16.0;\n"
    "vcolor=color;\n"
    "gl_PointSize=size;\n"
  "}\n"
"";

static const char akgl_fsrc_textile[]=
  "uniform vec2 screensize;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexoffset;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "vec2 texcoord=gl_PointCoord;\n"
    "texcoord.x=texcoord.x*2.0-0.5;\n"
    "if (texcoord.x<0.0) discard;\n"
    "if (texcoord.x>1.0) discard;\n"
    "texcoord=texcoord/16.0+vtexoffset;\n"
    "gl_FragColor=vec4(vcolor.rgb,texture2D(sampler,texcoord)*vcolor.a);\n"
  "}\n"
"";

/* Create shader.
 */

struct akgl_program *akgl_program_textile_new() {
  struct akgl_program *program=akgl_program_new();
  if (!program) return 0;

  if (akgl_program_compile(
    program,
    akgl_vsrc_textile,sizeof(akgl_vsrc_textile)-1,
    akgl_fsrc_textile,sizeof(akgl_fsrc_textile)-1
  )<0) {
    akgl_program_del(program);
    return 0;
  }

  return program;
}

/* Draw.
 */
 
int akgl_program_textile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_textile *vtxv,int vtxc) {
  if (!program||!texture) return -1;
  if (vtxc<1) return 0;
  if (!vtxv) return -1;
  
  if (akgl_program_use(program)<0) return -1;
  akgl_set_uniform_screen_size(program->location_screensize);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glEnable(GL_PROGRAM_POINT_SIZE);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct akgl_vtx_textile),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_textile),&vtxv[0].tileid);
  glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,1,sizeof(struct akgl_vtx_textile),&vtxv[0].r);
  glVertexAttribPointer(3,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_textile),&vtxv[0].size);
  glDrawArrays(GL_POINTS,0,vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);

  return 0;
}
