#ifndef PS_VIDEO_INTERNAL_H
#define PS_VIDEO_INTERNAL_H

#include "ps.h"
#include "ps_video.h"
#include "ps_video_layer.h"
#include "akgl/akgl.h"

#if PS_ARCH==PS_ARCH_macos
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#elif PS_ARCH==PS_ARCH_raspi
  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>
#elif PS_ARCH==PS_ARCH_linux
  #include <GL/gl.h>
#else
  #include <gl/gl.h>
#endif

/* Include provider. */
#if PS_USE_macwm
  #include "opt/macwm/ps_macwm.h"
#elif PS_USE_bcm
  #include "opt/bcm/ps_bcm.h"
#elif PS_USE_drm
  #include "opt/drm/ps_drm.h"
#elif PS_USE_glx
  #include "opt/glx/ps_glx.h"
#elif PS_USE_mswm
  #include "opt/mswm/ps_mswm.h"
#else
  #error "No video provider unit enabled."
#endif

#define PS_VIDEO_ASPECT_TOLERANCE 0.030

/* Globals.
 *****************************************************************************/

extern struct ps_video {
  int init;
  
  int winw,winh; // Size of the output graphics context (ie the main window)
  int dstx,dsty,dstw,dsth; // Boundaries for framebuffer copy, in (0,0)..(winw,winh).
  
  struct ps_video_layer **layerv;
  int layerc,layera;

  struct akgl_framebuffer *framebuffer;

  struct akgl_program *program_raw;
  struct akgl_program *program_fbxfer;
  struct akgl_program *program_tex;
  struct akgl_program *program_mintile;
  struct akgl_program *program_maxtile;
  struct akgl_program *program_textile;

  // A 12-point font generated entirely by code, no filesystem dependency.
  // This is always available as a fallback.
  struct akgl_texture *texture_minfont;

  // Shared vertex buffer.
  char *vtxv;
  int vtxc; // vertex count
  int vtxa; // *byte* count
  int vtxsize;

  // Vertex buffers for command coalesce.
  struct akgl_vtx_raw *vtxv_triangle;
  int vtxc_triangle,vtxa_triangle;
  struct akgl_vtx_textile *vtxv_textile;
  int vtxc_textile,vtxa_textile;
  struct akgl_vtx_maxtile *vtxv_maxtile;
  int vtxc_maxtile,vtxa_maxtile;
  uint8_t tsid_maxtile;
  struct akgl_vtx_mintile *vtxv_mintile;
  int vtxc_mintile,vtxa_mintile;
  uint8_t tsid_mintile;
  
} ps_video;

/* Private API.
 *****************************************************************************/

int ps_video_load_minfont(struct akgl_texture *texture);

/* To assemble a vertex list, begin with ps_video_vtxv_reset(sizeof(struct my_vertex_type)).
 * Then call ps_video_vtxv_add() to reallocate. It returns NULL or the first of the new vertices.
 * Whenever you're done, ps_video.vtxv and ps_video.vtxc are ready for use.
 */
int ps_video_vtxv_reset(int size);
void *ps_video_vtxv_add(int addc);

int ps_video_vtxv_triangle_require(int addc);
int ps_video_vtxv_textile_require(int addc);
int ps_video_vtxv_maxtile_require(int addc);
int ps_video_vtxv_mintile_require(int addc);

int ps_video_redraw_game_only();

#endif
