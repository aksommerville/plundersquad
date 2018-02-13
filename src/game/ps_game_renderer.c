#include "ps.h"
#include "ps_game_renderer.h"
#include "ps_game.h"
#include "ps_stats.h"
#include "ps_statusreport.h"
#include "video/ps_video.h"
#include "video/ps_video_layer.h"
#include "akgl/akgl.h"

#define PS_GAME_RENDERER_SLIDE_SPEED_X 12
#define PS_GAME_RENDERER_SLIDE_SPEED_Y  7

struct ps_game_layer {
  struct ps_video_layer hdr;
  struct ps_game *game;
  struct ps_game_renderer *renderer;
};

#define LAYER ((struct ps_game_layer*)layer)

static int ps_game_renderer_draw(struct ps_video_layer *layer);

/* Object lifecycle.
 */
 
struct ps_game_renderer *ps_game_renderer_new() {
  struct ps_game_renderer *renderer=calloc(1,sizeof(struct ps_game_renderer));
  if (!renderer) return 0;

  renderer->refc=1;

  if (!(renderer->visible_heroes=ps_sprgrp_new())) {
    ps_game_renderer_del(renderer);
    return 0;
  }

  return renderer;
}

void ps_game_renderer_del(struct ps_game_renderer *renderer) {
  if (!renderer) return;
  if (renderer->refc-->1) return;

  if (renderer->layer) {
    ps_video_uninstall_layer(renderer->layer);
    ps_video_layer_del(renderer->layer);
  }

  akgl_texture_del(renderer->capture);

  ps_sprgrp_clear(renderer->visible_heroes);
  ps_sprgrp_del(renderer->visible_heroes);

  free(renderer);
}

int ps_game_renderer_ref(struct ps_game_renderer *renderer) {
  if (!renderer) return -1;
  if (renderer->refc<1) return -1;
  if (renderer->refc==INT_MAX) return -1;
  renderer->refc++;
  return 0;
}

/* Setup.
 */

int ps_game_renderer_setup(struct ps_game_renderer *renderer,struct ps_game *game) {
  if (!renderer||!game) return -1;

  if (renderer->layer) {
    ps_video_uninstall_layer(renderer->layer);
    ps_video_layer_del(renderer->layer);
  }
  if (!(renderer->layer=ps_video_layer_new(sizeof(struct ps_game_layer)))) return -1;
  struct ps_video_layer *layer=renderer->layer;

  layer->blackout=1;
  layer->draw=ps_game_renderer_draw;
  LAYER->game=game;
  LAYER->renderer=renderer;

  if (ps_video_install_layer(renderer->layer,-1)<0) return -1;

  return 0;
}

/* Draw.
 */

static int ps_game_draw_hud(struct ps_game *game) {
  if (ps_video_text_begin()<0) return -1;
  if (ps_video_text_addf(12,0x000000ff,6,7,
    "%d/%d  %02d:%02d",
    ps_game_count_collected_treasures(game),game->treasurec,
    game->stats->playtime/3600,(game->stats->playtime/60)%60
  )<0) return -1;
  if (ps_video_text_end(0)<0) return -1;
  return 0;
}

static int ps_game_renderer_draw(struct ps_video_layer *layer) {
  struct ps_game *game=LAYER->game;
  struct ps_game_renderer *renderer=LAYER->renderer;

  if (renderer->slidex<0) {
    if ((renderer->slidex+=PS_GAME_RENDERER_SLIDE_SPEED_X)>0) renderer->slidex=0;
  } else if (renderer->slidex>0) {
    if ((renderer->slidex-=PS_GAME_RENDERER_SLIDE_SPEED_X)<0) renderer->slidex=0;
  }
  if (renderer->slidey<0) {
    if ((renderer->slidey+=PS_GAME_RENDERER_SLIDE_SPEED_Y)>0) renderer->slidey=0;
  } else if (renderer->slidey>0) {
    if ((renderer->slidey-=PS_GAME_RENDERER_SLIDE_SPEED_Y)<0) renderer->slidey=0;
  }
  
  if (game&&game->grid) {

    if (ps_video_draw_grid(game->grid,renderer->slidex,renderer->slidey)<0) return -1;
    if (game->statusreport) {
      if (ps_statusreport_draw(game->statusreport,renderer->slidex,renderer->slidey)<0) return -1;
    }
    if (ps_video_draw_sprites(game->grpv+PS_SPRGRP_VISIBLE,renderer->slidex,renderer->slidey)<0) return -1;

    if (!renderer->drawing_for_capture&&!renderer->slidex&&!renderer->slidey) {
      if (ps_game_draw_hud(game)<0) return -1;
    }

    if (!renderer->drawing_for_capture&&renderer->capture&&(renderer->slidex||renderer->slidey)) {
      int dstx=0,dsty=PS_SCREENH;
      if (renderer->slidex<0) dstx=PS_SCREENW+renderer->slidex;
      else if (renderer->slidex>0) dstx=renderer->slidex-PS_SCREENW;
      if (renderer->slidey<0) dsty=PS_SCREENH*2+renderer->slidey;
      else if (renderer->slidey>0) dsty=renderer->slidey;
      if (ps_video_draw_texture(renderer->capture,dstx,dsty,PS_SCREENW,-PS_SCREENH)<0) return -1;
    }
    
  }
  return 0;
}

/* Cache the set of visible heroes and remove all heroes from visible.
 */

static int ps_game_renderer_remove_heroes(struct ps_game_renderer *renderer,struct ps_game *game) {
  if (ps_sprgrp_clear(renderer->visible_heroes)<0) return -1;
  if (ps_sprgrp_intersect(renderer->visible_heroes,game->grpv+PS_SPRGRP_VISIBLE,game->grpv+PS_SPRGRP_HERO)<0) return -1;
  if (ps_sprgrp_remove_all(game->grpv+PS_SPRGRP_VISIBLE,renderer->visible_heroes)<0) return -1;
  return 0;
}

static int ps_game_renderer_restore_heroes(struct ps_game_renderer *renderer,struct ps_game *game) {
  if (ps_sprgrp_add_all(game->grpv+PS_SPRGRP_VISIBLE,renderer->visible_heroes)<0) return -1;
  if (ps_sprgrp_clear(renderer->visible_heroes)<0) return -1;
  return 0;
}

/* Begin slide.
 */
 
int ps_game_renderer_begin_slide(struct ps_game_renderer *renderer,int dx,int dy) {
  if (!renderer) return -1;
  if ((dx<-1)||(dx>1)||(dy<-1)||(dy>1)||(dx&&dy)) return -1;
  struct ps_game *game=((struct ps_game_layer*)renderer->layer)->game;

  if (renderer->capture) {
    akgl_texture_del(renderer->capture);
    renderer->capture=0;
  }

  if (ps_game_renderer_remove_heroes(renderer,game)<0) return -1;
  renderer->drawing_for_capture=1;
  if (ps_video_draw_to_framebuffer()<0) return -1;
  renderer->drawing_for_capture=0;
  if (ps_game_renderer_restore_heroes(renderer,game)<0) return -1;
  if (!(renderer->capture=ps_video_capture_framebuffer())) return -1;

  renderer->slidex=dx*PS_SCREENW;
  renderer->slidey=dy*PS_SCREENH;

  return 0;
}
