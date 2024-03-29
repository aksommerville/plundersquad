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

#define PS_GAME_RENDERER_FADE_FRAMEC_LIMIT 600

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

/* Fade.
 */

static uint32_t ps_game_renderer_get_fade_color(const struct ps_game_renderer *renderer) {
  if (renderer->fadec<1) return 0;
  if (renderer->fadep<=0) return renderer->fadergbaa;
  if (renderer->fadep>=renderer->fadec) return renderer->fadergbaz;
  double dsta=(double)renderer->fadep/(double)renderer->fadec;
  double srca=1.0-dsta;
  uint32_t rgba;
  const uint8_t *src=(uint8_t*)&renderer->fadergbaa;
  const uint8_t *dst=(uint8_t*)&renderer->fadergbaz;
  uint8_t *mix=(uint8_t*)&rgba;
  int i=0; for (;i<4;i++) {
    int n=src[i]*srca+dst[i]*dsta;
    if (n<=0) mix[i]=0;
    else if (n>=255) mix[i]=255;
    else mix[i]=n;
  }
  return rgba;
}
 
int ps_game_renderer_begin_fade(struct ps_game_renderer *renderer,uint32_t rgba,int framec) {
  if (!renderer) return -1;
  if (framec<0) return -1;
  if (framec>PS_GAME_RENDERER_FADE_FRAMEC_LIMIT) framec=PS_GAME_RENDERER_FADE_FRAMEC_LIMIT;
  if (framec) {
    renderer->fadergbaa=ps_game_renderer_get_fade_color(renderer);
    if (!renderer->fadergbaa) {
      renderer->fadergbaa=(rgba&0xffffff00);
    }
    renderer->fadergbaz=rgba;
    renderer->fadep=0;
    renderer->fadec=framec;
  } else {
    renderer->fadergbaa=rgba;
    renderer->fadergbaz=rgba;
    renderer->fadep=1;
    renderer->fadec=1;
  }
  return 0;
}

int ps_game_renderer_cancel_fade(struct ps_game_renderer *renderer) {
  if (!renderer) return -1;
  renderer->fadec=0;
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

  int slidex=renderer->slidex;
  int slidey=renderer->slidey;
  if (renderer->drawing_for_capture) {
    slidex=0;
    slidey=0;
  }

  if (renderer->fadep<renderer->fadec) {
    renderer->fadep++;
  }
  
  if (game&&game->grid) {
  
    if (ps_video_draw_grid(game->grid,slidex,slidey)<0) return -1;
  
    if (game->statusreport) {
      if (ps_statusreport_draw(game->statusreport,slidex,slidey)<0) return -1;
    }
  
    if (ps_video_draw_sprites(game->grpv+PS_SPRGRP_VISIBLE,slidex,slidey)<0) return -1;

    if (!renderer->drawing_for_capture&&renderer->capture&&(slidex||slidey)) {
      int dstx=0,dsty=PS_SCREENH;
      if (slidex<0) dstx=PS_SCREENW+slidex;
      else if (slidex>0) dstx=slidex-PS_SCREENW;
      if (slidey<0) dsty=PS_SCREENH*2+slidey;
      else if (slidey>0) dsty=slidey;
      if (ps_video_draw_texture(renderer->capture,dstx,dsty,PS_SCREENW,-PS_SCREENH)<0) return -1;
    }

    uint32_t rgba=ps_game_renderer_get_fade_color(renderer);
    if (rgba) {
      if (ps_video_draw_rect(0,0,PS_SCREENW,PS_SCREENH,rgba)<0) return -1;
    }

    if (!renderer->drawing_for_capture&&!slidex&&!slidey) {
      if (ps_game_draw_hud(game)<0) return -1;
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

/* Test whether this slide is reverse of current.
 */

static int ps_game_renderer_is_reverse(int dx,int dy,int slidex,int slidey) {
  if (!slidex&&!slidey) return 0;
  if ((dx<0)&&(slidex<0)) return 0;
  if ((dx>0)&&(slidex>0)) return 0;
  if ((dy<0)&&(slidey<0)) return 0;
  if ((dy>0)&&(slidey>0)) return 0;
  return 1;
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
  if (ps_video_draw_to_framebuffer()<0) {
    ps_log(VIDEO,ERROR,"Draw to framebuffer failed.");
    return -1;
  }
  renderer->drawing_for_capture=0;
  if (ps_game_renderer_restore_heroes(renderer,game)<0) return -1;
  if (!(renderer->capture=ps_video_capture_framebuffer())) return -1;

  if (ps_game_renderer_is_reverse(dx,dy,renderer->slidex,renderer->slidey)) {
    if (renderer->slidex<0) renderer->slidex+=PS_SCREENW;
    else if (renderer->slidex>0) renderer->slidex-=PS_SCREENW;
    if (renderer->slidey<0) renderer->slidey+=PS_SCREENH;
    else if (renderer->slidey>0) renderer->slidey-=PS_SCREENH;
  } else {
    renderer->slidex=dx*PS_SCREENW;
    renderer->slidey=dy*PS_SCREENH;
  }

  return 0;
}
