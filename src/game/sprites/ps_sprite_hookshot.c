#include "ps.h"
#include "ps_sprite_hookshot.h"
#include "ps_sprite_hero.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"

#define PS_HOOKSHOT_PHASE_INIT        0 /* Not fully initialized. */
#define PS_HOOKSHOT_PHASE_THROW       1 /* Going outward. */
#define PS_HOOKSHOT_PHASE_EMPTY       2 /* Returning empty. */
#define PS_HOOKSHOT_PHASE_DELIVER     3 /* Grabbed an object, returning it to user. */
#define PS_HOOKSHOT_PHASE_PULL        4 /* Latched on wall, pulling user to me. */

#define PS_HOOKSHOT_THROW_SPEED   3.0
#define PS_HOOKSHOT_EMPTY_SPEED   6.0
#define PS_HOOKSHOT_DELIVER_SPEED 5.0
#define PS_HOOKSHOT_PULL_SPEED    2.0

/* Expect myself and the pumpkin to be within this distance on each axis at the start of the update.
 * If so, we force the pumpkin to my position and hope it just works out.
 * If not, we drop the pumpkin, play the "bonk" sound, and return to player empty.
 */
#define PS_HOOKSHOT_DELIVERY_TOLERANCE 4.5

#define PS_HOOKSHOT_CHAIN_SIZE 10 /* Count of links in the chain, not counting the head. */

#define PS_HOOKSHOT_SPRDEF_ID 4

/* Private sprite object.
 */

struct ps_sprite_hookshot {
  struct ps_sprite hdr;
  double dx,dy; // Direction of throw, regardless of current phase.
  struct ps_sprgrp *user; // Should contain one sprite, the hero using me.
  struct ps_sprgrp *pumpkin;
  int phase;
  uint16_t restore_user_impassable;
  uint16_t restore_pumpkin_impassable;
  int initial_delivery; // Set when we enter DELIVER phase, to ignore the first pumpkin tolerance test.
};

#define SPR ((struct ps_sprite_hookshot*)spr)

/* Delete.
 */

static void _ps_hookshot_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->user);
  ps_sprgrp_del(SPR->user);
  ps_sprgrp_clear(SPR->pumpkin);
  ps_sprgrp_del(SPR->pumpkin);
}

/* Initialize.
 */

static int _ps_hookshot_init(struct ps_sprite *spr) {
  if (!(SPR->user=ps_sprgrp_new())) return -1;
  if (!(SPR->pumpkin=ps_sprgrp_new())) return -1;
  SPR->phase=PS_HOOKSHOT_PHASE_INIT;
  return 0;
}

/* Find a sprite in the given group.
 */

static struct ps_sprite *ps_hookshot_find_pumpkin(struct ps_sprite *spr,struct ps_sprgrp *grp) {
  double radius=PS_TILESIZE>>1;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=grp->sprv[i];
    double dx=spr->x-pumpkin->x; if (dx<0.0) dx=-dx;
    if (dx>=radius) continue;
    double dy=spr->y-pumpkin->y; if (dy<0.0) dy=-dy;
    if (dy>=radius) continue;
    return pumpkin;
  }
  return 0;
}

/* Just dropped the pumpkin at my feet.
 * If it's in a HOLE, do something special.
 */

static int ps_hookshot_check_pumpkin_final_position(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *pumpkin) {

  // Prizes don't care if they're over water or what.
  if (pumpkin->type==&ps_sprtype_prize) return 0;
  
  // Heroes handle this on their own.
  if (pumpkin->type==&ps_sprtype_hero) return 0;

  // All others will fall in the water if we drop at the right place.
  if (game->grid) {
    int col=pumpkin->x/PS_TILESIZE;
    int row=pumpkin->y/PS_TILESIZE;
    if ((col>=0)&&(row>=0)&&(col<PS_GRID_COLC)&&(row<PS_GRID_ROWC)) {
      const struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
      if (cell->physics==PS_BLUEPRINT_CELL_HOLE) {
        if (ps_game_create_splash(game,pumpkin->x,pumpkin->y)<0) return -1;
        ps_sprite_kill_later(pumpkin,game);
        if (ps_game_decorate_monster_death(game,pumpkin->x,pumpkin->y)<0) return -1;
      }
    }
  }
  
  return 0;
}

/* Status changes.
 */

static int ps_hookshot_begin_empty(struct ps_sprite *spr) {
  PS_SFX_HOOKSHOT_BONK
  SPR->phase=PS_HOOKSHOT_PHASE_EMPTY;
  return 0;
}

static int ps_hookshot_begin_pull(struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_HOOKSHOT_GRAB
  SPR->phase=PS_HOOKSHOT_PHASE_PULL;
  if (SPR->user&&(SPR->user->sprc==1)) {
    struct ps_sprite *user=SPR->user->sprv[0];
    if (ps_sprite_release_from_master(user,game)<0) return -1;
    if (ps_sprite_set_master(user,spr,game)<0) return -1;
    SPR->restore_user_impassable=user->impassable;
    if (user->type==&ps_sprtype_hero) {
      if (ps_hero_add_state(user,PS_HERO_STATE_PUMPKIN,game)<0) return -1;
      if (ps_hero_remove_state(user,PS_HERO_STATE_FERRY,game)<0) return -1;
    } else {
      user->impassable&=~(1<<PS_BLUEPRINT_CELL_HOLE);
    }
  }
  return 0;
}

static int ps_hookshot_begin_deliver(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *pumpkin) {
  PS_SFX_HOOKSHOT_GRAB
  SPR->phase=PS_HOOKSHOT_PHASE_DELIVER;
  SPR->initial_delivery=1;
  if (ps_sprite_set_master(pumpkin,spr,game)<0) return -1;
  if (ps_sprgrp_add_sprite(SPR->pumpkin,pumpkin)<0) return -1;
  if (pumpkin->type==&ps_sprtype_hero) {
    if (ps_hero_add_state(pumpkin,PS_HERO_STATE_PUMPKIN,game)<0) return -1;
  } else {
    SPR->restore_pumpkin_impassable=pumpkin->impassable;
    pumpkin->impassable&=~((1<<PS_BLUEPRINT_CELL_HOLE)|(1<<PS_BLUEPRINT_CELL_HEROONLY));
  }
  return 0;
}

static int ps_hookshot_drop_pumpkin(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->pumpkin&&(SPR->pumpkin->sprc>=1)) {
    struct ps_sprite *pumpkin=SPR->pumpkin->sprv[0];
    if (pumpkin->type==&ps_sprtype_hero) {
      if (ps_hero_remove_state(pumpkin,PS_HERO_STATE_PUMPKIN,game)<0) return -1;
    } else {
      pumpkin->impassable=SPR->restore_pumpkin_impassable;
    }
    if (ps_sprite_set_master(pumpkin,spr,game)<0) return -1;
    ps_sprgrp_clear(SPR->pumpkin);
    if (ps_hookshot_check_pumpkin_final_position(spr,game,pumpkin)<0) return -1;
  }
  return 0;
}

static int ps_hookshot_abort(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->user&&(SPR->user->sprc==1)) {
    ps_hero_remove_state(SPR->user->sprv[0],PS_HERO_STATE_HOOKSHOT,game);
    ps_hero_remove_state(SPR->user->sprv[0],PS_HERO_STATE_PUMPKIN,game);
  }
  if (ps_hookshot_drop_pumpkin(spr,game)<0) return -1;
  return ps_sprite_kill_later(spr,game);
}

static int ps_hookshot_finish(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->user&&(SPR->user->sprc==1)) {
    ps_hero_remove_state(SPR->user->sprv[0],PS_HERO_STATE_HOOKSHOT,game);
  }
  if (ps_hookshot_drop_pumpkin(spr,game)<0) return -1;
  return ps_sprite_kill_later(spr,game);
}

static int ps_hookshot_check_grab(struct ps_sprite *spr,struct ps_game *game) {

  /* Get grid cell. Return if offscreen. */
  if ((spr->x<0.0)||(spr->y<0.0)) return ps_hookshot_begin_empty(spr);
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) return ps_hookshot_begin_empty(spr);

  /* Have we hit a SOLID or LATCH cell? */
  if (game->grid) {
    uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
    switch (physics) {
      case PS_BLUEPRINT_CELL_SOLID: return ps_hookshot_begin_empty(spr);
      case PS_BLUEPRINT_CELL_LATCH: return ps_hookshot_begin_pull(spr,game);
    }
  }

  /* Have we hit a latchable sprite? */
  struct ps_sprite *pumpkin;
  if (pumpkin=ps_hookshot_find_pumpkin(spr,game->grpv+PS_SPRGRP_LATCH)) {
    return ps_hookshot_begin_deliver(spr,game,pumpkin);
  }
  if (pumpkin=ps_hookshot_find_pumpkin(spr,game->grpv+PS_SPRGRP_SOLID)) {
    return ps_hookshot_begin_empty(spr);
  }

  return 0;
}

static int ps_hookshot_check_return(struct ps_sprite *spr,struct ps_game *game) {

  /* Emergency. If we somehow ended up offscreen, abort. */
  if ((spr->x<0.0)||(spr->y<0.0)||(spr->x>PS_SCREENW)||(spr->y>PS_SCREENH)) {
    return ps_hookshot_abort(spr,game);
  }

  /* If we are within PS_TILESIZE of the user, finish. */
  if (!SPR->user||(SPR->user->sprc!=1)) return ps_hookshot_abort(spr,game);
  struct ps_sprite *user=SPR->user->sprv[0];
  double dx=spr->x-user->x;
  double dy=spr->y-user->y;
  if ((dx>=-PS_TILESIZE)&&(dx<=PS_TILESIZE)&&(dy>=-PS_TILESIZE)&&(dy<=PS_TILESIZE)) {
    return ps_hookshot_finish(spr,game);
  }

  return 0;
}

static int ps_hookshot_check_abortion(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->user) return ps_hookshot_abort(spr,game);
  if (SPR->user->sprc!=1) return ps_hookshot_abort(spr,game);
  struct ps_sprite *user=SPR->user->sprv[0];
  if (user->type!=&ps_sprtype_hero) return ps_hookshot_abort(spr,game);
  struct ps_sprite_hero *hero=(struct ps_sprite_hero*)user;
  if (hero->hp<1) return ps_hookshot_abort(spr,game);
  if (!(hero->state&PS_HERO_STATE_HOOKSHOT)) return ps_hookshot_abort(spr,game);
  return 0;
}

/* Ensure that we are in line with the user.
 * He can't walk while firing, but he might be riding a turtle or something.
 */

static int ps_hookshot_keep_line_straight(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->user->sprc!=1) return 0;
  struct ps_sprite *user=SPR->user->sprv[0];
  if (SPR->dx) {
    spr->y=user->y;
  } else if (SPR->dy) {
    spr->x=user->x;
  }
  return 0;
}

/* Update.
 */

static int _ps_hookshot_update(struct ps_sprite *spr,struct ps_game *game) {
  //ps_log(GAME,TRACE,"%s phase=%d d=(%f,%f)",__func__,SPR->phase,SPR->dx,SPR->dy);
  switch (SPR->phase) {
  
    case PS_HOOKSHOT_PHASE_THROW: {
        spr->x+=SPR->dx*PS_HOOKSHOT_THROW_SPEED;
        spr->y+=SPR->dy*PS_HOOKSHOT_THROW_SPEED;
        if (ps_hookshot_check_grab(spr,game)<0) return -1;
      } break;

    case PS_HOOKSHOT_PHASE_EMPTY: {
        spr->x-=SPR->dx*PS_HOOKSHOT_EMPTY_SPEED;
        spr->y-=SPR->dy*PS_HOOKSHOT_EMPTY_SPEED;
        if (ps_hookshot_check_return(spr,game)<0) return -1;
      } break;

    case PS_HOOKSHOT_PHASE_DELIVER: {
        if (SPR->pumpkin->sprc!=1) return ps_hookshot_begin_empty(spr);
        struct ps_sprite *pumpkin=SPR->pumpkin->sprv[0];
        double dx=pumpkin->x-spr->x; // Normally zero.
        double dy=pumpkin->y-spr->y;
        if (SPR->initial_delivery) dx=dy=0.0;
        if (
          (dx<-PS_HOOKSHOT_DELIVERY_TOLERANCE)||
          (dx>PS_HOOKSHOT_DELIVERY_TOLERANCE)||
          (dy<-PS_HOOKSHOT_DELIVERY_TOLERANCE)||
          (dy>PS_HOOKSHOT_DELIVERY_TOLERANCE)
        ) { // Delivery panic!
          spr->x=pumpkin->x;
          spr->y=pumpkin->y;
          if (ps_hookshot_drop_pumpkin(spr,game)<0) return -1;
          if (ps_hookshot_begin_empty(spr)<0) return -1;
        } else { // Normal delivery.
          spr->x-=SPR->dx*PS_HOOKSHOT_DELIVER_SPEED;
          spr->y-=SPR->dy*PS_HOOKSHOT_DELIVER_SPEED;
          pumpkin->x=spr->x;
          pumpkin->y=spr->y;
        }
        SPR->initial_delivery=0;
        if (ps_hookshot_check_return(spr,game)<0) return -1;
      } break;

    case PS_HOOKSHOT_PHASE_PULL: {
        if (SPR->user->sprc!=1) return ps_hookshot_abort(spr,game);
        struct ps_sprite *user=SPR->user->sprv[0];
        user->x+=SPR->dx*PS_HOOKSHOT_PULL_SPEED;
        user->y+=SPR->dy*PS_HOOKSHOT_PULL_SPEED;
        if (ps_hookshot_check_return(spr,game)<0) return -1;
      } break;
  }
  if (ps_hookshot_keep_line_straight(spr,game)<0) return -1;
  if (ps_hookshot_check_abortion(spr,game)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_hookshot_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (SPR->user->sprc!=1) return 0;
  struct ps_sprite *user=SPR->user->sprv[0];
  int vtxc=1+PS_HOOKSHOT_CHAIN_SIZE;
  if (vtxc>vtxa) return vtxc;

  /* Where on the user sprite are we connected? */
  double anchorx=user->x+user->radius*SPR->dx;
  double anchory=user->y+user->radius*SPR->dy;

  /* Initialize the first anchor sprite. */
  vtxv->x=anchorx;
  vtxv->y=anchory;
  vtxv->tileid=spr->tileid-0x10;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  /* Copy first sprite to all others and adjust positions linearly to spr->(x,y). */
  double dx=spr->x-user->x;
  double dy=spr->y-user->y;
  struct akgl_vtx_maxtile *vtx=vtxv+1;
  int i=1; for (;i<vtxc;i++,vtx++) {
    memcpy(vtx,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtx->x+=(dx*i)/PS_HOOKSHOT_CHAIN_SIZE;
    vtx->y+=(dy*i)/PS_HOOKSHOT_CHAIN_SIZE;
  }

  /* Overwrite tile and xform for the head vertex. */
  vtx=vtxv+vtxc-1;
  vtx->tileid=spr->tileid;
  if ((SPR->phase==PS_HOOKSHOT_PHASE_DELIVER)||(SPR->phase==PS_HOOKSHOT_PHASE_PULL)) vtx->tileid+=0x10;
  if (SPR->dx<0.0) vtx->xform=AKGL_XFORM_90;
  else if (SPR->dx>0.0) vtx->xform=AKGL_XFORM_270;
  else if (SPR->dy<0.0) vtx->xform=AKGL_XFORM_FLIP;
  
  return vtxc;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_hookshot={
  .name="hookshot",
  .objlen=sizeof(struct ps_sprite_hookshot),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=101,

  .init=_ps_hookshot_init,
  .del=_ps_hookshot_del,
  .update=_ps_hookshot_update,
  .draw=_ps_hookshot_draw,
  
};

/* Create new hookshot.
 */
 
struct ps_sprite *ps_sprite_hookshot_new(struct ps_sprite *user,struct ps_game *game) {
  if (!user||!game) return 0;
  if (user->type!=&ps_sprtype_hero) return 0;
  struct ps_sprite_hero *hero=(struct ps_sprite_hero*)user;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_HOOKSHOT_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found, expected for hookshot",PS_HOOKSHOT_SPRDEF_ID);
    return 0;
  }
  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,user->x,user->y);
  if (!spr) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",PS_HOOKSHOT_SPRDEF_ID);
    return 0;
  }

  if (ps_sprgrp_add_sprite(SPR->user,user)<0) return 0;
  switch (hero->facedir) {
    case PS_DIRECTION_NORTH: {
        SPR->dy=-1.0;
      } break;
    case PS_DIRECTION_SOUTH: {
        SPR->dy=1.0;
      } break;
    case PS_DIRECTION_WEST: {
        SPR->dx=-1.0;
      } break;
    case PS_DIRECTION_EAST: {
        SPR->dx=1.0;
      } break;
    default: return 0;
  }
  spr->x+=SPR->dx*(PS_TILESIZE>>1);
  spr->y+=SPR->dy*(PS_TILESIZE>>1);
  SPR->phase=PS_HOOKSHOT_PHASE_THROW;

  return spr;
}

/* Drop a slave sprite, either user or pumpkin.
 * Beware that (game) may be null.
 * All we do is empty the group containing (slave); our next update will clean it up.
 */
 
int ps_sprite_hookshot_drop_slave(struct ps_sprite *spr,struct ps_sprite *slave,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_hookshot)) return -1;
  if (!slave) return 0;
  if (SPR->user&&SPR->user->sprc&&(SPR->user->sprv[0]==slave)) {
    if (ps_sprgrp_remove_sprite(SPR->user,slave)<0) return -1;
  }
  if (SPR->pumpkin&&SPR->pumpkin->sprc&&(SPR->pumpkin->sprv[0]==slave)) {
    if (ps_sprgrp_remove_sprite(SPR->pumpkin,slave)<0) return -1;
  }
  return 0;
}
