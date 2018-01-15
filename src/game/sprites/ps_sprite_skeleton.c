/* ps_sprite_skeleton.c
 * Intended for use with a summoner.
 * Peeks head out of the ground, then pops out and runs around.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "akgl/akgl.h"

#define PS_SKELETON_PHASE_PEEKABOO_CRANIUM    0
#define PS_SKELETON_PHASE_PEEKABOO_PERISCOPE  1
#define PS_SKELETON_PHASE_WALK                2

#define PS_SKELETON_PEEKABOO_CRANIUM_TIME   60
#define PS_SKELETON_PERISCOPE_TIME_MIN      60
#define PS_SKELETON_PERISCOPE_TIME_MAX     120
#define PS_SKELETON_PERISCOPE_SIDE_TIME     30
#define PS_SKELETON_PERISCOPE_BACK_TIME     30
#define PS_SKELETON_WALK_FRAME_TIME          8

#define PS_SKELETON_WALK_TIME_MIN  30
#define PS_SKELETON_WALK_TIME_MAX 120
#define PS_SKELETON_WALK_SPEED    0.5

/* Private sprite object.
 */

struct ps_sprite_skeleton {
  struct ps_sprite hdr;
  int phase;
  int animcounter;
  int animframe;
  int walkcounter;
  int dx,dy;
};

#define SPR ((struct ps_sprite_skeleton*)spr)

/* Delete.
 */

static void _ps_skeleton_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_skeleton_init(struct ps_sprite *spr) {

  SPR->phase=PS_SKELETON_PHASE_PEEKABOO_CRANIUM;
  SPR->animcounter=PS_SKELETON_PEEKABOO_CRANIUM_TIME;

  return 0;
}

/* Configure.
 */

static int _ps_skeleton_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_skeleton_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_skeleton_configure(), for editor.
  return 0;
}

/* Begin walking.
 */

static int ps_skeleton_enter_walk(struct ps_sprite *spr,struct ps_game *game) {

  SPR->phase=PS_SKELETON_PHASE_WALK;
  SPR->animframe=0;
  SPR->animcounter=PS_SKELETON_WALK_FRAME_TIME;
  spr->layer=100;
  
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_PHYSICS,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_SOLID,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
  
  return 0;
}

/* Walk.
 */

static int ps_skeleton_update_walk(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->animcounter--<0) {
    SPR->animcounter=PS_SKELETON_WALK_FRAME_TIME;
    if (++(SPR->animframe)>1) {
      SPR->animframe=0;
    }
  }

  if (SPR->walkcounter--<0) {
    SPR->walkcounter=PS_SKELETON_WALK_TIME_MIN+rand()%(PS_SKELETON_WALK_TIME_MAX-PS_SKELETON_WALK_TIME_MIN);
    switch (rand()&3) {
      case 0: SPR->dx=0; SPR->dy=-1; break;
      case 1: SPR->dx=0; SPR->dy=1; break;
      case 2: SPR->dx=-1; SPR->dy=0; break;
      case 3: SPR->dx=1; SPR->dy=0; break;
    }
  }

  spr->x+=SPR->dx*PS_SKELETON_WALK_SPEED;
  spr->y+=SPR->dy*PS_SKELETON_WALK_SPEED;

  if ((spr->x<0)&&(SPR->dx<0)) SPR->dx=1;
  if ((spr->x>PS_SCREENW)&&(SPR->dx>0)) SPR->dx=-1;
  if ((spr->y<0)&&(SPR->dy<0)) SPR->dy=1;
  if ((spr->y>PS_SCREENH)&&(SPR->dy>0)) SPR->dy=-1;
  
  return 0;
}

/* Peekaboo.
 */

static int ps_skeleton_update_peekaboo_cranium(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->animcounter-->0) return 0;
  SPR->phase=PS_SKELETON_PHASE_PEEKABOO_PERISCOPE;
  SPR->animframe=0;
  SPR->animcounter=PS_SKELETON_PERISCOPE_TIME_MIN+rand()%(PS_SKELETON_PERISCOPE_TIME_MAX-PS_SKELETON_PERISCOPE_TIME_MIN);
  return 0;
}

static int ps_skeleton_update_peekaboo_periscope(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->animcounter--<0) {
    SPR->animframe++;
    switch (SPR->animframe) {
      case 1: SPR->animcounter=PS_SKELETON_PERISCOPE_SIDE_TIME; break;
      case 2: SPR->animcounter=PS_SKELETON_PERISCOPE_BACK_TIME; break;
      default: return ps_skeleton_enter_walk(spr,game);
    }
  }
  return 0;
}

/* Update.
 */

static int _ps_skeleton_update(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->phase) {
    case PS_SKELETON_PHASE_PEEKABOO_CRANIUM: if (ps_skeleton_update_peekaboo_cranium(spr,game)<0) return -1; break;
    case PS_SKELETON_PHASE_PEEKABOO_PERISCOPE: if (ps_skeleton_update_peekaboo_periscope(spr,game)<0) return -1; break;
    case PS_SKELETON_PHASE_WALK: if (ps_skeleton_update_walk(spr,game)<0) return -1; break;
  }
  return 0;
}

/* Draw.
 */

static int _ps_skeleton_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->xform=AKGL_XFORM_NONE;
  vtxv->t=0;

  switch (SPR->phase) {
    case PS_SKELETON_PHASE_PEEKABOO_CRANIUM: break;
    case PS_SKELETON_PHASE_PEEKABOO_PERISCOPE: switch (SPR->animframe) {
        case 0: vtxv->tileid+=1; break;
        case 1: vtxv->tileid+=1; vtxv->xform=AKGL_XFORM_FLOP; break;
        case 2: vtxv->tileid+=1; break;
      } break;
    case PS_SKELETON_PHASE_WALK: {
        switch (SPR->animframe) {
          case 0: vtxv->tileid+=3; break;
          case 1: vtxv->tileid+=4; break;
        }
      } break;
  }
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_skeleton={
  .name="skeleton",
  .objlen=sizeof(struct ps_sprite_skeleton),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_skeleton_init,
  .del=_ps_skeleton_del,
  .configure=_ps_skeleton_configure,
  .get_configure_argument_name=_ps_skeleton_get_configure_argument_name,
  .update=_ps_skeleton_update,
  .draw=_ps_skeleton_draw,

};
