/* ps_game_renderer.h
 * Manages rendering of the game, including transition effects.
 */

#ifndef PS_GAME_RENDERER_H
#define PS_GAME_RENDERER_H

struct ps_game;
struct ps_video_layer;
struct akgl_texture;

struct ps_game_renderer {
  int refc;
  struct ps_video_layer *layer;
  struct akgl_texture *capture;
  int slidex,slidey;
  int drawing_for_capture;
};

struct ps_game_renderer *ps_game_renderer_new();
void ps_game_renderer_del(struct ps_game_renderer *renderer);
int ps_game_renderer_ref(struct ps_game_renderer *renderer);

int ps_game_renderer_setup(struct ps_game_renderer *renderer,struct ps_game *game);

/* Capture the current image, then over the next few frames, slide new content in.
 * For transitions to neighbor grid.
 */
int ps_game_renderer_begin_slide(struct ps_game_renderer *renderer,int dx,int dy);

#endif
