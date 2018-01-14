#ifndef PS_VIDEO_INTERNAL_H
#define PS_VIDEO_INTERNAL_H

#include "ps.h"
#include "ps_video.h"
#include "ps_video_layer.h"
#include "akgl/akgl.h"

#if __APPLE__
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#else
  #include <gl/gl.h>
#endif

/* Include provider. */
#if PS_USE_macwm
  #include "opt/macwm/ps_macwm.h"
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

#endif
