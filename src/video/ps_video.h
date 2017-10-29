/* ps_video.h
 * We initialize the backend provider and set up an OpenGL context.
 */

#ifndef PS_VIDEO_H
#define PS_VIDEO_H

struct ps_video_layer;
struct ps_grid;
struct ps_sprgrp;

int ps_video_init();
void ps_video_quit();
int ps_video_is_init();

int ps_video_update();

/* Backend should call this when the window size changes.
 * This is not a request to resize; we don't do that.
 */
int ps_video_resized(int w,int h);

int ps_video_install_layer(struct ps_video_layer *layer,int p);
int ps_video_uninstall_layer(struct ps_video_layer *layer);
int ps_video_count_layers();
struct ps_video_layer *ps_video_get_layer(int p);

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

int ps_video_draw_grid(const struct ps_grid *grid);
int ps_video_draw_sprites(const struct ps_sprgrp *grp);

#endif
