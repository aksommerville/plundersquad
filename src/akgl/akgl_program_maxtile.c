#include "akgl_internal.h"

/* GLSL.
 */

static const char akgl_vsrc_maxtile[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 position;\n"
  "attribute float tile;\n"
  "attribute float size;\n"
  "attribute vec4 tint;\n"
  "attribute vec4 primary;\n"
  "attribute float rotation;\n"
  "attribute float xform;\n"
  "varying vec2 vtexoffset;\n"
  "varying vec4 vtint;\n"
  "varying vec4 vprimary;\n"
  "varying mat2 vxform;\n"
  "varying float vrotation;\n"
  "varying float vvxform;\n"
  "varying vec2 coordfudge;\n" // Ugly hack to get around point clipping. Without this, sprites disappear when halfway off screen.
  "void main() {\n"
    "gl_PointSize=size;\n"

    "coordfudge=vec2(0.0,0.0);\n"
    "if (position.y<0.0) {\n"
      "coordfudge.y=-position.y;\n"
    "} else if (position.y>=screensize.y) {\n"
      "coordfudge.y=screensize.y-position.y;\n"
    "}\n"
    "if (position.x<0.0) {\n"
      "coordfudge.x=-position.x;\n"
    "} else if (position.x>=screensize.x) {\n"
      "coordfudge.x=screensize.x-position.x;\n"
    "}\n"

    "vec2 adjposition=((position+coordfudge)*2.0)/screensize-1.0;\n"
    "adjposition.y=-adjposition.y;\n"
    "coordfudge/=size;\n"
    
    "gl_Position=vec4(adjposition,0.0,1.0);\n"
    "vtexoffset=vec2(floor(mod(tile+0.5,16.0)),floor((tile+0.5)/16.0))/16.0;\n"
    "vtint=tint;\n"
    "vprimary=primary;\n"

    "vrotation=rotation;\n"
    "if (rotation>0.0) {\n"
      "gl_PointSize*=1.41421356;\n"
      "float t=rotation*-6.283185307179586;\n"
      "vxform=mat2("
        "cos(t),-sin(t),"
        "sin(t),cos(t)"
      ");\n"
    "}\n"
    
    "vvxform=xform;\n"
  "}\n"
"";

static const char akgl_fsrc_maxtile[]=
  "uniform vec2 screensize;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexoffset;\n"
  "varying vec4 vtint;\n"
  "varying vec4 vprimary;\n"
  "varying mat2 vxform;\n"
  "varying float vrotation;\n"
  "varying float vvxform;\n"
  "varying vec2 coordfudge;\n"
  "void main() {\n"

    /* Fudge coordinates to get around edge clipping. */
    "vec2 texcoord=gl_PointCoord+coordfudge;\n"
    "if (texcoord.x<0.0) discard;\n"
    "if (texcoord.y<0.0) discard;\n"
    "if (texcoord.x>1.0) discard;\n"
    "if (texcoord.y>1.0) discard;\n"

    /* Adjust texture coordinates for rotation. */
    "if (vrotation>0.0) {\n"
      "texcoord*=1.41421356;\n"
      "texcoord=((texcoord-0.7071)*vxform).xy;\n"
      "texcoord+=0.5;\n"
      "if (texcoord.x<0.0) discard;\n"
      "if (texcoord.y<0.0) discard;\n"
      "if (texcoord.x>=1.0) discard;\n"
      "if (texcoord.y>=1.0) discard;\n"
    "}\n"

    /* Adjust texture coordinates for axis transforms. */
    "if (vvxform<1.0) {\n"
      // NONE
    "} else if (vvxform<2.0) {\n"
      // 90
      "texcoord=vec2(texcoord.y,1.0-texcoord.x);\n"
    "} else if (vvxform<3.0) {\n"
      // 180
      "texcoord=vec2(1.0-texcoord.x,1.0-texcoord.y);\n"
    "} else if (vvxform<4.0) {\n"
      // 270
      "texcoord=vec2(1.0-texcoord.y,texcoord.x);\n"
    "} else if (vvxform<5.0) {\n"
      // FLOP
      "texcoord.x=1.0-texcoord.x;\n"
    "} else if (vvxform<6.0) {\n"
      // FLOP90
      "texcoord=vec2(1.0-texcoord.y,1.0-texcoord.x);\n"
    "} else if (vvxform<7.0) {\n"
      // FLOP180
      "texcoord.y=1.0-texcoord.y;\n"
    "} else if (vvxform<8.0) {\n"
      // FLOP270
      "texcoord=vec2(texcoord.y,texcoord.x);\n"
    "}\n"
    
    "if (texcoord.x<0.0) discard;\n"
    "if (texcoord.y<0.0) discard;\n"
    "if (texcoord.x>=1.0) discard;\n"
    "if (texcoord.y>=1.0) discard;\n"

    /* Read color from texture. */
    "texcoord=texcoord/16.0+vtexoffset;\n"
    "vec4 color=texture2D(sampler,texcoord);\n"

    /* Apply master alpha. */
    "color.a*=vprimary.a;\n"

    /* Apply primary color if gray. */
    "if ((color.r==color.g)&&(color.g==color.b)) {\n"
      "if (color.r<0.5) {\n"
        "color=vec4(mix(vec3(0.0,0.0,0.0),vprimary.rgb,color.r*2.0),color.a);\n"
      "} else if (color.r>0.5) {\n"
        "color=vec4(mix(vprimary.rgb,vec3(1.0,1.0,1.0),(color.r-0.5)*2.0),color.a);\n"
      "} else {\n"
        "color=vec4(vprimary.rgb,color.a);\n"
      "}\n"
    "}\n"

    /* Apply tint. */
    "if (vtint.a>0.0) {\n"
      "color=vec4(mix(color.rgb,vtint.rgb,vtint.a),color.a);\n"
    "}\n"

    "gl_FragColor=color;\n"
  "}\n"
"";

/* Create shader.
 */

struct akgl_program *akgl_program_maxtile_new() {
  struct akgl_program *program=akgl_program_new();
  if (!program) return 0;

  if (akgl_program_compile(
    program,
    akgl_vsrc_maxtile,sizeof(akgl_vsrc_maxtile)-1,
    akgl_fsrc_maxtile,sizeof(akgl_fsrc_maxtile)-1
  )<0) {
    const char *log=0;
    int logc=akgl_program_get_error_log(&log,program);
    if (log&&(logc>0)) {
      ps_log(VIDEO,ERROR,"Failed to compile 'maxtile' shader...\n%.*s\n",logc,log);
    }
    akgl_program_del(program);
    return 0;
  }

  return program;
}

/* Draw.
 */
 
int akgl_program_maxtile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_maxtile *vtxv,int vtxc) {
  if (!program||!texture) return -1;
  if (vtxc<1) return 0;
  if (!vtxv) return -1;
  
  if (akgl_program_use(program)<0) return -1;
  #if PS_ARCH!=PS_ARCH_raspi
    glEnable(GL_TEXTURE_2D);
  #endif
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  #ifdef GL_PROGRAM_POINT_SIZE
    glEnable(GL_PROGRAM_POINT_SIZE);
  #endif
  #ifdef GL_POINT_SPRITE
    glEnable(GL_POINT_SPRITE);
  #endif

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  glEnableVertexAttribArray(5);
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct akgl_vtx_maxtile),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_maxtile),&vtxv[0].tileid);
  glVertexAttribPointer(2,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_maxtile),&vtxv[0].size);
  glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,1,sizeof(struct akgl_vtx_maxtile),&vtxv[0].tr);
  glVertexAttribPointer(4,4,GL_UNSIGNED_BYTE,1,sizeof(struct akgl_vtx_maxtile),&vtxv[0].pr);
  glVertexAttribPointer(5,1,GL_UNSIGNED_BYTE,1,sizeof(struct akgl_vtx_maxtile),&vtxv[0].t);
  glVertexAttribPointer(6,1,GL_UNSIGNED_BYTE,0,sizeof(struct akgl_vtx_maxtile),&vtxv[0].xform);
  glDrawArrays(GL_POINTS,0,vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);
  glDisableVertexAttribArray(4);
  glDisableVertexAttribArray(5);
  glDisableVertexAttribArray(6);

  return 0;
}
