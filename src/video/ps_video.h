/* ps_video.h
 * We initialize the backend provider and set up an OpenGL context.
 */

#ifndef PS_VIDEO_H
#define PS_VIDEO_H

struct ps_video_layer;
struct ps_grid;
struct ps_sprgrp;
struct akgl_vtx_mintile;
struct akgl_vtx_maxtile;
struct akgl_vtx_raw;
struct akgl_texture;
struct ps_cmdline;

int ps_video_init(const struct ps_cmdline *cmdline);
void ps_video_quit();
int ps_video_is_init();

int ps_video_update();

/* Draw one frame as usual but stop after filling the framebuffer.
 */
int ps_video_draw_to_framebuffer();

/* Capture the most recent frame as an akgl_texture or raw RGBA.
 * Raw capture on success returns the length in bytes, which is always (PS_SCREENW*PS_SCREENH*4).
 * (game_only) means redraw the buffer first, with GUI layers removed.
 */
struct akgl_texture *ps_video_capture_framebuffer();
int ps_video_capture_framebuffer_raw(void *pixelspp,int game_only);

/* Backend should call this when the window size changes.
 * This is not a request to resize; we don't do that.
 */
int ps_video_resized(int w,int h);

int ps_video_install_layer(struct ps_video_layer *layer,int p);
int ps_video_uninstall_layer(struct ps_video_layer *layer);
int ps_video_count_layers();
struct ps_video_layer *ps_video_get_layer(int p);

int ps_video_point_framebuffer_from_window(int *fbx,int *fby,int winx,int winy);
int ps_video_point_window_from_framebuffer(int *winx,int *winy,int fbx,int fby);

/* Helpers for drawing.
 *****************************************************************************/

int ps_video_draw_rect(int x,int y,int w,int h,uint32_t rgba);
int ps_video_draw_horz_gradient(int x,int y,int w,int h,uint32_t rgba_left,uint32_t rgba_right);
int ps_video_draw_vert_gradient(int x,int y,int w,int h,uint32_t rgba_top,uint32_t rgba_bottom);

int ps_video_text_begin();
int ps_video_text_add(int size,uint32_t rgba,int x,int y,const char *src,int srcc);
int ps_video_text_addf(int size,uint32_t rgba,int x,int y,const char *fmt,...);
int ps_video_text_addfv(int size,uint32_t rgba,int x,int y,const char *fmt,va_list vargs);
int ps_video_text_end(int resid);

int ps_video_draw_grid(const struct ps_grid *grid,int offx,int offy);
int ps_video_draw_sprites(const struct ps_sprgrp *grp,int offx,int offy);

int ps_video_draw_mintile(const struct akgl_vtx_mintile *vtxv,int vtxc,uint8_t tsid);
int ps_video_draw_maxtile(const struct akgl_vtx_maxtile *vtxv,int vtxc,uint8_t tsid);
int ps_video_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc);
int ps_video_draw_texture(struct akgl_texture *texture,int x,int y,int w,int h);

#endif
