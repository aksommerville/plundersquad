#include "ps.h"
#include "ps_sprite_hero.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"
#include "util/ps_geometry.h"

#define PS_ARROW_SPRDEF_ID 5
#define PS_ARROW_SPEED 4.0

/* Private sprite object.
 */

struct ps_sprite_arrow {
  struct ps_sprite hdr;
  double dx,dy;
  double left,top,right,bottom; // Physical bounds, relative to (x,y)
  struct ps_sprgrp *user;
};

#define SPR ((struct ps_sprite_arrow*)spr)

/* Delete, init.
 */

static void _ps_arrow_del(struct ps_sprite *spr) {
  ps_sprgrp_clear(SPR->user);
  ps_sprgrp_del(SPR->user);
}

static int _ps_arrow_init(struct ps_sprite *spr) {
  if (!(SPR->user=ps_sprgrp_new())) return -1;
  return 0;
}

/* Check if we can kill something.
 */

static int ps_arrow_kill_mortals(struct ps_sprite *spr,struct ps_game *game) {
  double left=spr->x+SPR->left;
  double right=spr->x+SPR->right;
  double top=spr->y+SPR->top;
  double bottom=spr->y+SPR->bottom;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_FRAGILE].sprc;i++) {
    struct ps_sprite *victim=game->grpv[PS_SPRGRP_FRAGILE].sprv[i];
    if (victim->x+victim->radius<=left) continue;
    if (victim->x-victim->radius>=right) continue;
    if (victim->y+victim->radius<=top) continue;
    if (victim->y-victim->radius>=bottom) continue;
    struct ps_sprite *assailant=spr;
    if (SPR->user&&(SPR->user->sprc==1)) assailant=SPR->user->sprv[0];
    if (ps_sprite_receive_damage(game,victim,assailant)<0) return -1;
    if (ps_sprite_kill_later(spr,game)<0) return -1;
    return 1;
  }
  return 0;
}

/* Abort if we hit something solid.
 */

static int ps_arrow_check_collisions(struct ps_sprite *spr,struct ps_game *game) {

  if ((spr->x<0.0)||(spr->y<0.0)) return ps_sprite_kill_later(spr,game);
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) return ps_sprite_kill_later(spr,game);

  if (game->grid) {
    uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
    switch (physics) {
      case PS_BLUEPRINT_CELL_LATCH:
      case PS_BLUEPRINT_CELL_SOLID:
        return ps_sprite_kill_later(spr,game);
    }
  }

  //TODO arrows maybe should stop at solid sprites too

  return 0;
}

/* Update.
 */

static int _ps_arrow_update(struct ps_sprite *spr,struct ps_game *game) {
  int err;
  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  if (err=ps_arrow_kill_mortals(spr,game)) return err;
  if (ps_arrow_check_collisions(spr,game)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_arrow_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=spr->tileid;
  vtxv->pr=0x80;
  vtxv->pg=0x80;
  vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->ta=0x00;
  vtxv->t=0;
  if (SPR->dx<0.0) vtxv->xform=AKGL_XFORM_90;
  else if (SPR->dx>0.0) vtxv->xform=AKGL_XFORM_270;
  else if (SPR->dy<0.0) vtxv->xform=AKGL_XFORM_FLIP;
  else vtxv->xform=AKGL_XFORM_NONE;
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_arrow={
  .name="arrow",
  .objlen=sizeof(struct ps_sprite_arrow),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .del=_ps_arrow_del,
  .init=_ps_arrow_init,
  .update=_ps_arrow_update,
  .draw=_ps_arrow_draw,
  
};

/* New, public ctor.
 */
 
struct ps_sprite *ps_sprite_arrow_new(struct ps_sprite *user,struct ps_game *game) {
  if (!user||!game) return 0;
  if (user->type!=&ps_sprtype_hero) return 0;
  struct ps_sprite_hero *hero=(struct ps_sprite_hero*)user;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_ARROW_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found, expected for ARROW",PS_ARROW_SPRDEF_ID);
    return 0;
  }
  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,user->x,user->y);
  if (!spr) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",PS_ARROW_SPRDEF_ID);
    return 0;
  }

  if (ps_sprgrp_add_sprite(SPR->user,user)<0) return 0;
  switch (hero->facedir) {
    case PS_DIRECTION_NORTH: {
        SPR->dy=-1.0;
        SPR->left=-2.0;
        SPR->right=2.0;
        SPR->top=-7.0;
        SPR->bottom=-7.0;
      } break;
    case PS_DIRECTION_SOUTH: {
        SPR->dy=1.0;
        SPR->left=-2.0;
        SPR->right=2.0;
        SPR->top=-7.0;
        SPR->bottom=-7.0;
      } break;
    case PS_DIRECTION_WEST: {
        SPR->dx=-1.0;
        SPR->left=-7.0;
        SPR->right=7.0;
        SPR->top=-2.0;
        SPR->bottom=-2.0;
      } break;
    case PS_DIRECTION_EAST: {
        SPR->dx=1.0;
        SPR->left=-7.0;
        SPR->right=7.0;
        SPR->top=-2.0;
        SPR->bottom=-2.0;
      } break;
    default: return 0;
  }
  spr->x+=SPR->dx*PS_TILESIZE;
  spr->y+=SPR->dy*PS_TILESIZE;
  SPR->dx*=PS_ARROW_SPEED;
  SPR->dy*=PS_ARROW_SPEED;

  return spr;
}
