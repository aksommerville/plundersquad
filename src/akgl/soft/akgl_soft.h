/* akgl_soft.h
 * Software emulation of our GL 2.x functionality.
 * Consumers shouldn't need to use this interface.
 * Just ask for AKGL_STRATEGY_SOFT at init.
 */

#ifndef AKGL_SOFT_H
#define AKGL_SOFT_H

struct ps_sdraw_image;

/* This is the only program substitute function that can be used without a framebuffer selected.
 */
int akgl_soft_fbxfer_draw(struct ps_sdraw_image *fb,int x,int y,int w,int h);

int akgl_soft_maxtile_draw(struct akgl_texture *texture,const struct akgl_vtx_maxtile *vtxv,int vtxc);
int akgl_soft_mintile_draw(struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size);

// XXX Decide whether we actually want 1:1 correspondence with the akgl interface.
// In any case, ps_video_draw will use ps_sdraw directly where warranted.
int akgl_soft_raw_draw_triangle_strip(const struct akgl_vtx_raw *vtxv,int vtxc);
int akgl_soft_raw_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc,int width);
int akgl_soft_raw_draw_points(const struct akgl_vtx_raw *vtxv,int vtxc,int size);
int akgl_soft_tex_draw(struct akgl_texture *tex,const struct akgl_vtx_tex *vtxv,int vtxc);
int akgl_soft_textile_draw(struct akgl_texture *tex,const struct akgl_vtx_textile *vtxv,int vtxc);

#endif
