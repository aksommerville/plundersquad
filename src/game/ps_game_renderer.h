/* ps_game_renderer.h
 * Manages rendering of the game, including transition effects.
 */

#ifndef PS_GAME_RENDERER_H
#define PS_GAME_RENDERER_H

struct ps_game;
struct ps_video_layer;
struct akgl_texture;
struct ps_sprgrp;

struct ps_game_renderer {
  int refc;
  struct ps_video_layer *layer;
  struct akgl_texture *capture;
  int slidex,slidey;
  int drawing_for_capture;
  struct ps_sprgrp *visible_heroes;
  int fadep,fadec;
  uint32_t fadergbaa,fadergbaz;
};

struct ps_game_renderer *ps_game_renderer_new();
void ps_game_renderer_del(struct ps_game_renderer *renderer);
int ps_game_renderer_ref(struct ps_game_renderer *renderer);

int ps_game_renderer_setup(struct ps_game_renderer *renderer,struct ps_game *game);

/* Capture the current image, then over the next few frames, slide new content in.
 * For transitions to neighbor grid.
 */
int ps_game_renderer_begin_slide(struct ps_game_renderer *renderer,int dx,int dy);

/* Draw an overlay between sprites and HUD, of color (rgba).
 * Begins transparent and fades in over (framec) frames, starting now.
 * If a fade is active, go from the current state.
 * Begin a fade with (framec==0) to take effect immediately.
 */
int ps_game_renderer_begin_fade(struct ps_game_renderer *renderer,uint32_t rgba,int framec);
int ps_game_renderer_cancel_fade(struct ps_game_renderer *renderer);

#endif
