/* akgl.h
 * Wrapper to OpenGL or OpenGL ES.
 * You're on your own for linkage; it varies from platform to platform.
 */

#ifndef AKGL_H
#define AKGL_H

#include <stdint.h>

struct akgl_texture;
struct akgl_framebuffer;
struct akgl_program;

#define AKGL_FMT_RGB8        1
#define AKGL_FMT_RGBA8       2
#define AKGL_FMT_Y8          3
#define AKGL_FMT_YA8         4
#define AKGL_FMT_A8          5

/* Axis transforms. Consider a human head in profile for these descriptions... */
#define AKGL_XFORM_NONE      0 /* Hair up, nose right. */
#define AKGL_XFORM_90        1 /* Hair right, nose down. */
#define AKGL_XFORM_180       2 /* Hair down, nose left. */
#define AKGL_XFORM_270       3 /* Hair left, nose up. */
#define AKGL_XFORM_FLOP      4 /* Hair up, nose left. */
#define AKGL_XFORM_FLOP90    5 /* Hair right, nose up. */
#define AKGL_XFORM_FLOP180   6 /* Hair down, nose right. */
#define AKGL_XFORM_FLOP270   7 /* Hair left, nose down. */
#define AKGL_XFORM_FLIP AKGL_XFORM_FLOP180

/* Global rendering context.
 *****************************************************************************/

int akgl_init();
void akgl_quit();

/* This must be kept up to date, we apply it when switching from framebuffer to main out.
 */
int akgl_set_screen_size(int w,int h);
int akgl_get_screen_size(int *w,int *h);

int akgl_set_glsl_version(int version);

int akgl_get_error();
int akgl_clear_error();

int akgl_clear(uint32_t rgba);

/* Texture.
 *****************************************************************************/

struct akgl_texture *akgl_texture_new();
struct akgl_texture *akgl_texture_from_framebuffer(struct akgl_framebuffer *framebuffer);
void akgl_texture_del(struct akgl_texture *texture);
int akgl_texture_ref(struct akgl_texture *texture);

int akgl_texture_get_fmt(const struct akgl_texture *texture);
int akgl_texture_get_size(int *w,int *h,const struct akgl_texture *texture);
int akgl_texture_get_filter(const struct akgl_texture *texture);
int akgl_texture_set_filter(struct akgl_texture *texture,int filter);

int akgl_texture_load(
  struct akgl_texture *texture,
  const void *pixels,
  int fmt,int w,int h
);

int akgl_texture_realloc(
  struct akgl_texture *texture,
  int fmt,int w,int h
);

int akgl_texture_load_sub(
  struct akgl_texture *texture,
  const void *pixels,
  int x,int y,int w,int h
);

int akgl_texture_use(struct akgl_texture *texture);

/* Framebuffer.
 *****************************************************************************/

struct akgl_framebuffer *akgl_framebuffer_new();
void akgl_framebuffer_del(struct akgl_framebuffer *framebuffer);
int akgl_framebuffer_ref(struct akgl_framebuffer *framebuffer);

int akgl_framebuffer_resize(struct akgl_framebuffer *framebuffer,int w,int h);

int akgl_framebuffer_use(struct akgl_framebuffer *framebuffer);

/* Program.
 *****************************************************************************/

struct akgl_program *akgl_program_new();
void akgl_program_del(struct akgl_program *program);
int akgl_program_ref(struct akgl_program *program);

int akgl_program_compile(
  struct akgl_program *program,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc
);

int akgl_program_handoff_error_log(struct akgl_program *program,char *log,int logc);
int akgl_program_get_error_log(void *dstpp,const struct akgl_program *program);

int akgl_program_use(struct akgl_program *program);

/* Common built-in programs.
 *  - raw: Draw 2-D primitives with pure colors.
 *  - fbxfer: Copy a framebuffer's texture.
 *  - tex: Draw textured 2-D triangle strips.
 *  - mintile: Axis-aligned point sprites from 16x16 tilesheet.
 *  - maxtile: Point sprites with all the bells and whistles.
 *  - textile: Axis-aligned 1:2 point sprites tuned for text.
 * You are of course free to add custom programs, but these six should cover everything I want to do.
 *****************************************************************************/

struct akgl_vtx_raw {
  int16_t x,y;
  uint8_t r,g,b,a;
};

struct akgl_program *akgl_program_raw_new();
int akgl_program_raw_draw_triangle_strip(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc);
int akgl_program_raw_draw_line_strip(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc,int width);
int akgl_program_raw_draw_points(struct akgl_program *program,const struct akgl_vtx_raw *vtxv,int vtxc,int size);

struct akgl_program *akgl_program_fbxfer_new();
int akgl_program_fbxfer_draw(struct akgl_program *program,struct akgl_framebuffer *framebuffer,int x,int y,int w,int h);

struct akgl_vtx_tex {
  int16_t x,y;
  uint8_t tx,ty;
};

struct akgl_program *akgl_program_tex_new();
int akgl_program_tex_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_tex *vtxv,int vtxc);

struct akgl_vtx_mintile {
  int16_t x,y;
  uint8_t tileid;
};

struct akgl_program *akgl_program_mintile_new();
int akgl_program_mintile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_mintile *vtxv,int vtxc,int size);

struct akgl_vtx_maxtile {
  int16_t x,y;
  uint8_t tileid;
  uint8_t size;
  uint8_t tr,tg,tb,ta; // Tint towards (tr,tg,tb) by (ta)
  uint8_t pr,pg,pb; // Pure gray are replaced by this; (128,128,128) is no-op.
  uint8_t a; // Alpha multiplier.
  uint8_t t; // Rotation clockwise in (0..255)
  uint8_t xform; // Axis transform; See constants above.
};

struct akgl_program *akgl_program_maxtile_new();
int akgl_program_maxtile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_maxtile *vtxv,int vtxc);

struct akgl_vtx_textile {
  int16_t x,y;
  uint8_t tileid;
  uint8_t r,g,b,a; // Texture's color is ignored, and its alpha multiplied by (a).
  uint8_t size; // Height; width is half.
};

struct akgl_program *akgl_program_textile_new();
int akgl_program_textile_draw(struct akgl_program *program,struct akgl_texture *texture,const struct akgl_vtx_textile *vtxv,int vtxc);

#endif
