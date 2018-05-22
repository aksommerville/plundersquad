#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "game/sprites/ps_sprite_hero.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"

#define PS_EXPLOSION_SPRDEFID 7

#define PS_LANDMINE_COLOR_A 0xff0000
#define PS_LANDMINE_COLOR_B 0xffff00
#define PS_LANDMINE_TIME_A  20
#define PS_LANDMINE_TIME_B  40

/* Private sprite object.
 */

struct ps_sprite_landmine {
  struct ps_sprite hdr;
  int frame;
  int clock;
  int blown;
};

#define SPR ((struct ps_sprite_landmine*)spr)

/* Animate.
 */

static int ps_landmine_animate(struct ps_sprite *spr) {
  if (--(SPR->clock)<=0) {
    if (++(SPR->frame)>1) SPR->frame=0;
    switch (SPR->frame) {
      case 0: SPR->clock=PS_LANDMINE_TIME_A; break;
      case 1: SPR->clock=PS_LANDMINE_TIME_B; break;
    }
  }
  return 0;
}

/* Create explosion.
 */

static int ps_landmine_create_explosion(const struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_EXPLODE
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_EXPLOSION_SPRDEFID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for landmine explosion",PS_EXPLOSION_SPRDEFID);
    return -1;
  }
  struct ps_sprite *explosion=ps_sprdef_instantiate(game,sprdef,0,0,spr->x,spr->y);
  if (!explosion) return -1;
  return 0;
}

/* Examine pigeon. Is it not explodable?
 */

static int ps_landmine_should_ignore_pigeon(const struct ps_sprite *spr,const struct ps_game *game,const struct ps_sprite *pigeon) {

  if (pigeon->type==&ps_sprtype_hero) {
    if (!ps_hero_stateq_feet_on_ground(pigeon)) return 1;
  }

  return 0;
}

/* Explode if anyone is stepping on me.
 */

static int ps_landmine_check_feet(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->blown) return 0;
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_SOLID;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pigeon=grp->sprv[i];
    if (pigeon->x<=left) continue;
    if (pigeon->x>=right) continue;
    if (pigeon->y<=top) continue;
    if (pigeon->y>=bottom) continue;
    if (ps_landmine_should_ignore_pigeon(spr,game,pigeon)) continue;

    SPR->blown=1;
    if (ps_landmine_create_explosion(spr,game)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    return 0;
    
  }
  return 0;
}

/* Update.
 */

static int _ps_landmine_update(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_landmine_animate(spr)<0) return -1;
  if (ps_landmine_check_feet(spr,game)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_landmine_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->a=0xff;
  vtxv->ta=0;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  switch (SPR->frame) {
    case 0: {
        vtxv->pr=PS_LANDMINE_COLOR_A>>16;
        vtxv->pg=PS_LANDMINE_COLOR_A>>8;
        vtxv->pb=PS_LANDMINE_COLOR_A;
      } break;
    case 1: {
        vtxv->pr=PS_LANDMINE_COLOR_B>>16;
        vtxv->pg=PS_LANDMINE_COLOR_B>>8;
        vtxv->pb=PS_LANDMINE_COLOR_B;
      } break;
  }
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_landmine={
  .name="landmine",
  .objlen=sizeof(struct ps_sprite_landmine),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=10,

  .update=_ps_landmine_update,
  .draw=_ps_landmine_draw,
  
};
