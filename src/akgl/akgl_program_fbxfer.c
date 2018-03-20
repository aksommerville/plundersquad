#include "akgl_internal.h"

#if PS_NO_OPENGL2

struct akgl_program *akgl_program_fbxfer_new() { return 0; }

int akgl_program_fbxfer_draw(struct akgl_program *program,struct akgl_framebuffer *framebuffer,int x,int y,int w,int h) {
  return akgl_soft_fbxfer_draw((struct ps_sdraw_image*)framebuffer,x,y,w,h);
}

#else

/* GLSL.
 */

struct akgl_vtx_fbxfer {
  int16_t x,y;
  uint8_t tx,ty;
};

static const char akgl_vsrc_fbxfer[]=
  "uniform vec2 screensize;\n"
  "varying vec2 vtexcoord;\n"
  "attribute vec2 position;\n"
  "attribute vec2 texcoord;\n"
  "void main() {\n"
    "vec2 adjposition=(position*2.0)/screensize-1.0;\n"
    "adjposition.y=-adjposition.y;\n"
    "gl_Position=vec4(adjposition,0.0,1.0);\n"
    "vtexcoord=texcoord;\n"
  "}\n"
"";

static const char akgl_fsrc_fbxfer[]=
  "uniform vec2 screensize;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
"";

/* Create shader.
 */
 
struct akgl_program *akgl_program_fbxfer_new() {
  struct akgl_program *program=akgl_program_new();
  if (!program) return 0;

  if (akgl_program_compile(
    program,
    akgl_vsrc_fbxfer,sizeof(akgl_vsrc_fbxfer)-1,
    akgl_fsrc_fbxfer,sizeof(akgl_fsrc_fbxfer)-1
  )<0) {
    const char *log=0;
    int logc=akgl_program_get_error_log(&log,program);
    if (log&&(logc>0)) {
      ps_log(VIDEO,ERROR,"Failed to compile 'fbxfer' shader...\n%.*s\n",logc,log);
    }
    akgl_program_del(program);
    return 0;
  }

  return program;
}

/* Draw.
 */

int akgl_program_fbxfer_draw(struct akgl_program *program,struct akgl_framebuffer *framebuffer,int x,int y,int w,int h) {

  if (akgl.strategy==AKGL_STRATEGY_SOFT) {
    return akgl_soft_fbxfer_draw((struct ps_sdraw_image*)framebuffer,x,y,w,h);
  }

  if (!program||!framebuffer) return -1;
  if ((w<1)||(h<1)) return 0;
  
  if (akgl_program_use(program)<0) return -1;
  akgl_set_uniform_screen_size(program->location_screensize);
  #if PS_ARCH!=PS_ARCH_raspi
    glEnable(GL_TEXTURE_2D);
  #endif
  glBindTexture(GL_TEXTURE_2D,framebuffer->texid);

  struct akgl_vtx_fbxfer vtxv[4]={
    {x  ,y  ,0,1},
    {x  ,y+h,0,0},
    {x+w,y  ,1,1},
    {x+w,y+h,1,0},
  };

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct akgl_vtx_fbxfer),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_fbxfer),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  return 0;
}

#endif
