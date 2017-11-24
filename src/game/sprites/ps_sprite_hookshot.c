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

#define PS_HOOKSHOT_CHAIN_SIZE 10 /* Count of links in the chain, not counting the head. */

#define PS_HOOKSHOT_SPRDEF_ID 4

/* Private sprite object.
 */

struct ps_sprite_hookshot {
  struct ps_sprite hdr;
  double dx,dy; // Direction of throw, regardless of current phase.
  struct ps_sprgrp *user; // Should contain one sprite, the hero using me.
  struct ps_sprgrp *pumpkin;
  int pumpkin_restore_collide_hole;
  int phase;
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

/* Status changes.
 */

static int ps_hookshot_begin_empty(struct ps_sprite *spr) {
  PS_SFX_HOOKSHOT_BONK
  SPR->phase=PS_HOOKSHOT_PHASE_EMPTY;
  return 0;
}

static int ps_hookshot_begin_pull(struct ps_sprite *spr) {
  PS_SFX_HOOKSHOT_GRAB
  SPR->phase=PS_HOOKSHOT_PHASE_PULL;
  if (SPR->user&&(SPR->user->sprc==1)) {
    struct ps_sprite *user=SPR->user->sprv[0];
    user->collide_hole=0;
  }
  return 0;
}

static int ps_hookshot_begin_deliver(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *pumpkin) {
  PS_SFX_HOOKSHOT_GRAB
  SPR->phase=PS_HOOKSHOT_PHASE_DELIVER;
  SPR->pumpkin_restore_collide_hole=pumpkin->collide_hole;
  pumpkin->collide_hole=0;
  if (ps_sprgrp_add_sprite(SPR->pumpkin,pumpkin)<0) return -1;
  return 0;
}

static int ps_hookshot_abort(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->user&&(SPR->user->sprc==1)) ps_hero_abort_hookshot(SPR->user->sprv[0],game);
  if (SPR->pumpkin&&(SPR->pumpkin->sprc==1)) SPR->pumpkin->sprv[0]->collide_hole=SPR->pumpkin_restore_collide_hole;
  return ps_sprite_kill_later(spr,game);
}

static int ps_hookshot_finish(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->user&&(SPR->user->sprc==1)) ps_hero_abort_hookshot(SPR->user->sprv[0],game);
  if (SPR->pumpkin&&(SPR->pumpkin->sprc==1)) SPR->pumpkin->sprv[0]->collide_hole=SPR->pumpkin_restore_collide_hole;
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
      case PS_BLUEPRINT_CELL_LATCH: return ps_hookshot_begin_pull(spr);
    }
  }

  /* Have we hit a latchable sprite? */
  struct ps_sprite *pumpkin;
  if (pumpkin=ps_hookshot_find_pumpkin(spr,game->grpv+PS_SPRGRP_LATCH)) {
    return ps_hookshot_begin_deliver(spr,game,pumpkin);
  }
  if (pumpkin=ps_hookshot_find_pumpkin(spr,game->grpv+PS_SPRGRP_PHYSICS)) {
    return ps_hookshot_begin_empty(spr);
  }

  return 0;
}

static int ps_hookshot_check_return(struct ps_sprite *spr,struct ps_game *game) {

  /* Emergency. If we somehow ended up offscreen, abort. */
  if ((spr->x<0.0)||(spr->y<0.0)||(spr->x>PS_SCREENW)||(spr->y>PS_SCREENH)) {
    return ps_hookshot_abort(spr,game);
  }

  /* If we are within PS_TILSIZE of the user, finish. */
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
  if (!hero->hookshot_in_progress) return ps_hookshot_abort(spr,game);
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
        spr->x-=SPR->dx*PS_HOOKSHOT_DELIVER_SPEED;
        spr->y-=SPR->dy*PS_HOOKSHOT_DELIVER_SPEED;
        pumpkin->x=spr->x;
        pumpkin->y=spr->y;
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
