/* ps_widget_sprite.c
 * Draw a sprite, just like the game does.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "game/ps_sprite.h"
#include "game/sprites/ps_sprite_hero.h"
#include "video/ps_video.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "input/ps_input_record.h"
#include "input/ps_input_button.h"

/* Object definition.
 */

struct ps_widget_sprite {
  struct ps_widget hdr;
  struct ps_sprgrp *grp;
  struct ps_game *game; // Dummy to fool the sprite.
  struct ps_player *player; // Automatically attached to hero sprites.
  struct ps_input_record *record;
  int plrdefid;
};

#define WIDGET ((struct ps_widget_sprite*)widget)

/* Recorded input.
 */

static struct ps_input_record *ps_sprite_build_input_record() {
  struct ps_input_record *record=ps_input_record_new();
  if (ps_input_record_add_event(record,  0,PS_PLRBTN_DOWN)<0) return 0;
  if (ps_input_record_add_event(record, 10,0)<0) return 0;
  if (ps_input_record_add_event(record,100,PS_PLRBTN_DOWN)<0) return 0;
  if (ps_input_record_add_event(record, 20,0)<0) return 0;
  if (ps_input_record_add_event(record, 60,PS_PLRBTN_A)<0) return 0;
  if (ps_input_record_add_event(record, 10,0)<0) return 0;
  if (ps_input_record_add_event(record,100,PS_PLRBTN_LEFT)<0) return 0;
  if (ps_input_record_add_event(record, 20,0)<0) return 0;
  if (ps_input_record_add_event(record, 60,PS_PLRBTN_A)<0) return 0;
  if (ps_input_record_add_event(record, 10,0)<0) return 0;
  if (ps_input_record_add_event(record,100,PS_PLRBTN_RIGHT)<0) return 0;
  if (ps_input_record_add_event(record, 20,0)<0) return 0;
  if (ps_input_record_add_event(record, 60,PS_PLRBTN_A)<0) return 0;
  if (ps_input_record_add_event(record,  0,0)<0) return 0;
  return record;
}

/* Create a mock game object to permit updating sprite.
 * Don't use ps_game_new(); it interacts with video and input.
 */

static struct ps_game *ps_sprite_create_mock_game() {
  struct ps_game *game=calloc(1,sizeof(struct ps_game));
  if (!game) return 0;
  return game;
}

/* Delete.
 */

static void _ps_sprite_del(struct ps_widget *widget) {
  ps_sprgrp_clear(WIDGET->grp);
  ps_sprgrp_del(WIDGET->grp);
  ps_player_del(WIDGET->player);
  ps_game_del(WIDGET->game);
  ps_input_record_del(WIDGET->record);
}

/* Initialize.
 */

static int _ps_sprite_init(struct ps_widget *widget) {

  WIDGET->plrdefid=1;

  if (!(WIDGET->grp=ps_sprgrp_new())) return -1;
  if (!(WIDGET->game=ps_sprite_create_mock_game())) return -1;
  
  if (!(WIDGET->player=ps_player_new())) return -1;
  if (!(WIDGET->player->plrdef=ps_res_get(PS_RESTYPE_PLRDEF,WIDGET->plrdefid))) return -1;
  WIDGET->player->playerid=-1; // Prevent automatic input retrieval

  if (!(WIDGET->record=ps_sprite_build_input_record())) return -1;

  return 0;
}

/* Draw.
 */

static void ps_sprite_position_all(struct ps_sprgrp *grp,int x,int y) {
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *sprite=grp->sprv[i];
    sprite->x=x;
    sprite->y=y;
  }
}

static int _ps_sprite_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  if (WIDGET->grp&&(WIDGET->grp->sprc>0)) {
    ps_sprite_position_all(WIDGET->grp,parentx+widget->x+(widget->w>>1),parenty+widget->y+(widget->h>>1));
    int err=ps_video_draw_sprites(WIDGET->grp,0,0);
    if (err<0) return err;
  }
  
  return 0;
}

/* Measure.
 * Sprites don't generically report their visible size.
 * Most are square, PS_TILESIZE.
 * But some, in particular 'hero', which is the only we care about, are larger.
 * Request double PS_TILESIZE to be safe.
 */

static int _ps_sprite_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_TILESIZE<<1;
  *h=PS_TILESIZE<<1;
  return 0;
}

/* Pack.
 */

static int _ps_sprite_pack(struct ps_widget *widget) {
  int i=WIDGET->grp->sprc; while (i-->0) {
    struct ps_sprite *sprite=WIDGET->grp->sprv[i];
    sprite->x=widget->w>>1;
    sprite->y=widget->h>>1;
  }
  return 0;
}

/* Update.
 */

static int _ps_sprite_update(struct ps_widget *widget) {
  int i=WIDGET->grp->sprc; while (i-->0) {
    struct ps_sprite *sprite=WIDGET->grp->sprv[i];

    if (sprite->type==&ps_sprtype_hero) {
      if (ps_hero_accept_fake_input(sprite,ps_input_record_update(WIDGET->record),WIDGET->game)<0) return -1;
    }
    
    if (ps_sprite_update(sprite,WIDGET->game)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_sprite={

  .name="sprite",
  .objlen=sizeof(struct ps_widget_sprite),

  .del=_ps_sprite_del,
  .init=_ps_sprite_init,

  .draw=_ps_sprite_draw,
  .measure=_ps_sprite_measure,
  .pack=_ps_sprite_pack,

  .update=_ps_sprite_update,

};

/* If we have any hero sprites, send zero input to them.
 */

static int ps_sprite_drop_input(struct ps_widget *widget) {
  int i=WIDGET->grp->sprc; while (i-->0) {
    struct ps_sprite *sprite=WIDGET->grp->sprv[i];

    if (sprite->type==&ps_sprtype_hero) {
      if (ps_hero_accept_fake_input(sprite,0,WIDGET->game)<0) return -1;
    }
    
    if (ps_sprite_update(sprite,WIDGET->game)<0) return -1;
  }
  return 0;
}

/* Replace sprite.
 */
 
int ps_widget_sprite_load_sprdef(struct ps_widget *widget,int sprdefid) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;

  if (ps_sprite_drop_input(widget)<0) return -1;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
  if (!sprdef) {
    ps_log(GUI,ERROR,"sprdef:%d not found",sprdefid);
    return 0;
  }
  
  struct ps_sprite *sprite=ps_sprdef_instantiate(WIDGET->game,sprdef,0,0,0,0);
  if (!sprite) return -1;
  if (ps_sprgrp_kill(WIDGET->grp)<0) return -1;
  if (ps_sprgrp_add_sprite(WIDGET->grp,sprite)<0) return -1;

  if (sprite->type==&ps_sprtype_hero) {
    if (ps_hero_set_player(sprite,WIDGET->player)<0) return -1;
  }

  return 0;
}

/* Set plrdefid.
 */
 
int ps_widget_sprite_set_plrdefid(struct ps_widget *widget,int plrdefid) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;
  
  if (ps_sprite_drop_input(widget)<0) return -1;

  if (!(WIDGET->player->plrdef=ps_res_get(PS_RESTYPE_PLRDEF,plrdefid))) {
    ps_log(GUI,ERROR,"plrdef:%d not found",plrdefid);
    return -1;
  }

  WIDGET->plrdefid=plrdefid;

  if (WIDGET->player->palette>=WIDGET->player->plrdef->palettec) {
    WIDGET->player->palette=0;
  }

  return 0;
}

/* Set palette.
 */
 
int ps_widget_sprite_set_palette(struct ps_widget *widget,int palette) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;
  if ((palette<0)||(palette>=WIDGET->player->plrdef->palettec)) {
    WIDGET->player->palette=0;
  } else {
    WIDGET->player->palette=palette;
  }
  return 0;
}

/* Get plrdefid or palette.
 */
 
int ps_widget_sprite_get_plrdefid(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;
  return WIDGET->plrdefid;
}

int ps_widget_sprite_get_palette(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;
  return WIDGET->player->palette;
}

/* Modify plrdefid or palette.
 */
 
int ps_widget_sprite_modify_plrdefid(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;
  
  if (ps_sprite_drop_input(widget)<0) return -1;

  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_PLRDEF);
  if (!restype) return -1;
  if (restype->resc<1) {
    ps_log(GUI,ERROR,"No plrdef resources found!");
    return -1;
  }

  int resp=ps_restype_res_search(restype,WIDGET->plrdefid);
  if (resp<0) {
    resp=0;
  } else {
    if (d<0) resp--;
    else resp++;
    if (resp<0) resp=restype->resc-1;
    else if (resp>=restype->resc) resp=0;
  }

  WIDGET->plrdefid=restype->resv[resp].id;
  WIDGET->player->plrdef=restype->resv[resp].obj;

  if (WIDGET->player->palette>=WIDGET->player->plrdef->palettec) {
    WIDGET->player->palette=WIDGET->player->plrdef->palettec-1;
  }

  return 0;
}

int ps_widget_sprite_modify_palette(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_sprite)) return -1;

  if (!WIDGET->player->plrdef||(WIDGET->player->plrdef->palettec<2)) {
    WIDGET->player->palette=0;
    return 0;
  }

  if (d<0) WIDGET->player->palette--;
  else WIDGET->player->palette++;
  if (WIDGET->player->palette<0) {
    WIDGET->player->palette=WIDGET->player->plrdef->palettec-1;
  } else if (WIDGET->player->palette>=WIDGET->player->plrdef->palettec) {
    WIDGET->player->palette=0;
  }
  
  return 0;
}
