/* ps_sprite_inert.h
 * This is basically a dummy sprite, but it adds a 'fling' behavior so the swordsman can bat it around.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "util/ps_geometry.h"

#define PS_INERT_DEFAULT_SPEED 3.0
#define PS_INERT_STOP_SPEED 2.0 /* Actual movement less than this, we stop. */

/* Private sprite object.
 */

struct ps_sprite_inert {
  struct ps_sprite hdr;
  double pvx,pvy;
  int dx,dy;
  double speed;
};

#define SPR ((struct ps_sprite_inert*)spr)

/* Update.
 */

static int _ps_inert_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->dx||SPR->dy) {
    double adx=(spr->x>SPR->pvx)?(spr->x-SPR->pvx):(SPR->pvx-spr->x);
    double ady=(spr->y>SPR->pvy)?(spr->y-SPR->pvy):(SPR->pvy-spr->y);
    double actual_travel=adx+ady;
    if (actual_travel<=PS_INERT_STOP_SPEED) {
      SPR->dx=0;
      SPR->dy=0;
    } else {
      SPR->pvx=spr->x;
      SPR->pvy=spr->y;
      spr->x+=SPR->speed*SPR->dx;
      spr->y+=SPR->speed*SPR->dy;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_inert={
  .name="inert",
  .objlen=sizeof(struct ps_sprite_inert),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .update=_ps_inert_update,

};

/* Fling.
 */
 
int ps_sprite_inert_fling(struct ps_sprite *spr,struct ps_game *game,int dir) {
  if (!spr||(spr->type!=&ps_sprtype_inert)) return -1;
  switch (dir) {
    case PS_DIRECTION_NORTH: SPR->dx=0; SPR->dy=-1; break;
    case PS_DIRECTION_SOUTH: SPR->dx=0; SPR->dy=1; break;
    case PS_DIRECTION_WEST: SPR->dx=-1; SPR->dy=0; break;
    case PS_DIRECTION_EAST: SPR->dx=1; SPR->dy=0; break;
    default: return -1;
  }
  SPR->speed=PS_INERT_DEFAULT_SPEED;
  // Adjust (pvx,pvy) to appear as if we are already moving.
  SPR->pvx=spr->x-SPR->dx*SPR->speed;
  SPR->pvy=spr->y-SPR->dy*SPR->speed;
  return 0;
}
