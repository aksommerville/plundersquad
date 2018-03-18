/* akgl_soft.h
 * Software emulation of our GL 2.x functionality.
 * Consumers shouldn't need to use this interface.
 * Just ask for AKGL_STRATEGY_SOFT at init.
 */

#ifndef AKGL_SOFT_H
#define AKGL_SOFT_H

struct akgl_texture_soft;
struct akgl_framebuffer_soft;
struct akgl_program_soft;

/* Texture.
 *********************************************************/

struct akgl_texture_soft *akgl_texture_soft_new();
void akgl_texture_soft_del(struct akgl_texture_soft *tex);
int akgl_texture_soft_ref(struct akgl_texture_soft *tex);

int akgl_texture_soft_get_fmt(const struct akgl_texture_soft *tex);
int akgl_texture_soft_get_size(int *w,int *h,const struct akgl_texture_soft *tex);

struct akgl_texture_soft *akgl_texture_soft_from_framebuffer(struct akgl_framebuffer_soft *fb);

int akgl_texture_soft_load(
  struct akgl_texture_soft *tex,
  const void *pixels,
  int fmt,int w,int h
);

int akgl_texture_soft_realloc(
  struct akgl_texture_soft *tex,
  int fmt,int w,int h
);

int akgl_texture_soft_load_sub(
  struct akgl_texture_soft *tex,
  const void *pixels,
  int x,int y,int w,int h
);

int akgl_fmt_get_pixel_size(int fmt);

/* Framebuffer.
 *********************************************************/

struct akgl_framebuffer_soft *akgl_framebuffer_soft_new();
void akgl_framebuffer_soft_del(struct akgl_framebuffer_soft *fb);
int akgl_framebuffer_soft_ref(struct akgl_framebuffer_soft *fb);
int akgl_framebuffer_soft_resize(struct akgl_framebuffer_soft *fb,int w,int h);
int akgl_framebuffer_soft_use(struct akgl_framebuffer_soft *fb);

int akgl_framebuffer_soft_blit(
  struct akgl_framebuffer_soft *dst,int dstx,int dsty,int dstw,int dsth,
  const struct akgl_texture_soft *src,int srcx,int srcy,int srcw,int srch
);

int akgl_framebuffer_soft_blit_replacergb(
  struct akgl_framebuffer_soft *dst,int dstx,int dsty,int dstw,int dsth,
  const struct akgl_texture_soft *src,int srcx,int srcy,int srcw,int srch,
  uint32_t rgba
);

/* Program. (XXX?)
 *********************************************************/

struct akgl_program_soft *akgl_program_soft_new();
void akgl_program_soft_del(struct akgl_program_soft *program);
int akgl_program_soft_ref(struct akgl_program_soft *program);

/* Program substitute functions.
 *********************************************************/

/* This is the only program substitute function that can be used without a framebuffer selected.
 */
int akgl_soft_fbxfer_draw(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h);

int akgl_soft_maxtile_draw(struct akgl_texture *texture,const struct akgl_vtx_maxtile *vtxv,int vtxc);
int akgl_soft_mintile_draw(struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size);
int akgl_soft_raw_draw_triangle_strip(const struct akgl_vtx_raw *vtxv,int vtxc);
int akgl_soft_raw_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc,int width);
int akgl_soft_raw_draw_points(const struct akgl_vtx_raw *vtxv,int vtxc,int size);
int akgl_soft_tex_draw(struct akgl_texture *tex,const struct akgl_vtx_tex *vtxv,int vtxc);
int akgl_soft_textile_draw(struct akgl_texture *tex,const struct akgl_vtx_textile *vtxv,int vtxc);

/* Rendering primitives.
 ***********************************************************/

int akgl_soft_draw_rect(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba);
int akgl_soft_draw_vert_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba_n,uint32_t rgba_s);
int akgl_soft_draw_horz_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t rgba_w,uint32_t rgba_e);
int akgl_soft_draw_2d_gradient(struct akgl_framebuffer_soft *fb,int x,int y,int w,int h,uint32_t nw,uint32_t ne,uint32_t sw,uint32_t se);

#endif
