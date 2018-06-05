/* Arrow with a hero's head on it, indicating that someone is offscreen.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "akgl/akgl.h"
#include "util/ps_geometry.h"
#include "game/sprites/ps_sprite_hero.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"

#define PS_HEROINDICATOR_DISPLACEMENT_TIME  80
#define PS_HEROINDICATOR_ALPHA_TIME         30
#define PS_HEROINDICATOR_DISPLACEMENT_MIN    8
#define PS_HEROINDICATOR_DISPLACEMENT_MAX   20

/* Private sprite object.
 */

struct ps_sprite_heroindicator {
  struct ps_sprite hdr;
  struct ps_sprgrp *hero;
  int direction;
  int displacement_counter;
  int alpha_counter;
};

#define SPR ((struct ps_sprite_heroindicator*)spr)

/* Delete.
 */

static void _ps_heroindicator_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->hero);
  ps_sprgrp_del(SPR->hero);
}

/* Initialize.
 */

static int _ps_heroindicator_init(struct ps_sprite *spr) {

  spr->tsid=0x03;

  if (!(SPR->hero=ps_sprgrp_new())) return -1;

  return 0;
}

/* Update.
 */

static int _ps_heroindicator_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->displacement_counter)>=PS_HEROINDICATOR_DISPLACEMENT_TIME) {
    SPR->displacement_counter=0;
  }
  if (++(SPR->alpha_counter)>=PS_HEROINDICATOR_ALPHA_TIME) {
    SPR->alpha_counter=0;
  }

  if (!SPR->hero||(SPR->hero->sprc<1)) {
    return ps_sprite_kill_later(spr,game);
  }

  struct ps_sprite *hero=SPR->hero->sprv[0];

  /* We are only pointing to heroes when offscreen.
   * So this algorithm is simple: Sit on top of the hero, then clamp myself onscreen.
   */
  const double xlo=PS_TILESIZE>>1;
  const double ylo=PS_TILESIZE>>1;
  const double xhi=PS_SCREENW-(PS_TILESIZE>>1);
  const double yhi=PS_SCREENH-(PS_TILESIZE>>1);
  spr->x=hero->x;
  spr->y=hero->y;
  if (spr->x<xlo) {
    spr->x=xlo;
    SPR->direction=PS_DIRECTION_WEST;
  } else if (spr->x>xhi) {
    spr->x=xhi;
    SPR->direction=PS_DIRECTION_EAST;
  }
  if (spr->y<ylo) {
    spr->y=ylo;
    SPR->direction=PS_DIRECTION_NORTH;
  } else if (spr->y>yhi) {
    spr->y=yhi;
    SPR->direction=PS_DIRECTION_SOUTH;
  }

  return 0;
}

/* Draw.
 */

static int _ps_heroindicator_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  const struct ps_sprite *hero=0;
  const struct ps_sprite_hero *HERO=0;
  const struct ps_player *player=0;
  const struct ps_plrdef *plrdef=0;
  if (SPR->hero&&SPR->hero->sprc) {
    hero=SPR->hero->sprv[0];
    if (hero->type==&ps_sprtype_hero) {
      HERO=(struct ps_sprite_hero*)hero;
      if (player=HERO->player) {
        if (player->plrdef) {
          plrdef=HERO->player->plrdef;
        }
      }
    }
  }
  int vtxc=1+(plrdef?1:0);
  if (vtxc>vtxa) return vtxc;

  int dx=0,dy=0;
  int displacement;
  int displacement_range=PS_HEROINDICATOR_DISPLACEMENT_MAX-PS_HEROINDICATOR_DISPLACEMENT_MIN+1;
  if (SPR->displacement_counter<PS_HEROINDICATOR_DISPLACEMENT_TIME>>1) {
    displacement=PS_HEROINDICATOR_DISPLACEMENT_MIN+(SPR->displacement_counter*displacement_range)/(PS_HEROINDICATOR_DISPLACEMENT_TIME>>1);
  } else {
    displacement=PS_HEROINDICATOR_DISPLACEMENT_MIN+((PS_HEROINDICATOR_DISPLACEMENT_TIME-SPR->displacement_counter)*displacement_range)/(PS_HEROINDICATOR_DISPLACEMENT_TIME>>1);
  }

  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].tileid=0xcc;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=(SPR->alpha_counter*0xff)/PS_HEROINDICATOR_ALPHA_TIME;
  vtxv[0].t=0;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv[0].xform=AKGL_XFORM_FLIP; dy=1; break;
    case PS_DIRECTION_WEST: vtxv[0].xform=AKGL_XFORM_90; dx=1; break;
    case PS_DIRECTION_EAST: vtxv[0].xform=AKGL_XFORM_270; dx=-1; break;
    case PS_DIRECTION_SOUTH: default: vtxv[0].xform=AKGL_XFORM_NONE; dy=-1;
  }

  if (plrdef) {
    vtxv[1].x=vtxv[0].x+dx*displacement;
    vtxv[1].y=vtxv[0].y+dy*displacement;
    vtxv[1].tileid=plrdef->tileid_head;
    vtxv[1].size=PS_TILESIZE;
    vtxv[1].ta=0;
    vtxv[1].a=0xc0;
    vtxv[1].t=0;
    vtxv[1].xform=AKGL_XFORM_NONE;

    uint32_t rgb=0x808080;
    if ((player->palette>=0)&&(player->palette<plrdef->palettec)) {
      rgb=plrdef->palettev[player->palette].rgba_head>>8;
    }
    vtxv[1].pr=rgb>>16;
    vtxv[1].pg=rgb>>8;
    vtxv[1].pb=rgb;

    if (HERO&&HERO->highlighttime) {
      vtxv[1].tr=0xff;
      vtxv[1].tg=0xff;
      vtxv[1].tb=0x00;
      vtxv[1].ta=(HERO->highlighttime*255)/PS_HERO_HIGHLIGHT_TIME;
    }
  }
  
  return vtxc;
}

/* Hurt (dummy).
 */

static int _ps_heroindicator_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailiant) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_heroindicator={
  .name="heroindicator",
  .objlen=sizeof(struct ps_sprite_heroindicator),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=150,

  .init=_ps_heroindicator_init,
  .del=_ps_heroindicator_del,
  .update=_ps_heroindicator_update,
  .draw=_ps_heroindicator_draw,
  .hurt=_ps_heroindicator_hurt,
  
};

/* Public API.
 */
 
struct ps_sprite *ps_sprite_heroindicator_get_hero(const struct ps_sprite *spr) {
  if (!spr) return 0;
  if (spr->type!=&ps_sprtype_heroindicator) return 0;
  if (!SPR->hero) return 0;
  if (SPR->hero->sprc<1) return 0;
  return SPR->hero->sprv[0];
}

int ps_sprite_heroindicator_set_hero(struct ps_sprite *spr,struct ps_sprite *hero) {
  if (!spr||(spr->type!=&ps_sprtype_heroindicator)) return -1;
  if (ps_sprgrp_clear(SPR->hero)<0) return -1;
  if (hero) {
    if (ps_sprgrp_add_sprite(SPR->hero,hero)<0) return -1;
  }
  return 0;
}
