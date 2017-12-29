#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"

/* Private sprite object.
 */

struct ps_sprite_switch {
  struct ps_sprite hdr;
  int barrierid;
  int state; // Final answer: Am I on or off?
  int stompbox; // Permanent role: Am I a stompbox or a treadle plate?
  int press; // Is someone standing on me right now?
};

#define SPR ((struct ps_sprite_switch*)spr)

/* Configure.
 */

static int _ps_switch_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc) {
  if (argc>=1) {
    SPR->barrierid=argv[0];
    if (argc>=2) {
      SPR->stompbox=argv[1];
    }
  }
  return 0;
}

static const char *_ps_switch_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "Barrier ID";
    case 1: return "Stompbox?";
  }
  return 0;
}

/* Poll for desired state.
 */

static int ps_switch_poll_state(struct ps_sprite *spr,struct ps_game *game) {
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_PHYSICS].sprc;i++) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_PHYSICS].sprv[i];
    if (!hero->collide_hole) continue; // Feet not touching ground.
    if (hero->x<=left) continue;
    if (hero->x>=right) continue;
    if (hero->y<=top) continue;
    if (hero->y>=bottom) continue;
    return 1;
  }
  return 0;
}

/* Toggle.
 */

static int ps_switch_engage(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->state) return 0;
  SPR->state=1;
//  spr->tileid++;
  if (ps_grid_open_barrier(game->grid,SPR->barrierid)<0) return -1;
  return 0;
}

static int ps_switch_disengage(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->state) return 0;
  SPR->state=0;
//  spr->tileid--;
  if (ps_grid_close_barrier(game->grid,SPR->barrierid)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_switch_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Terminate if weight upon me hasn't changed. */
  int state=ps_switch_poll_state(spr,game);
  if (state==SPR->press) return 0;

  /* Newly pressed. */
  if (SPR->press=state) {
    PS_SFX_SWITCH_PRESS
    spr->tileid++;
    if (SPR->stompbox) {
      if (SPR->state) {
        if (ps_switch_disengage(spr,game)<0) return -1;
      } else {
        if (ps_switch_engage(spr,game)<0) return -1;
      }
    } else {
      if (ps_switch_engage(spr,game)<0) return -1;
    }

  /* Newly released. */
  } else {
    PS_SFX_SWITCH_RELEASE
    spr->tileid--;
    if (!SPR->stompbox) {
      if (ps_switch_disengage(spr,game)<0) return -1;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_switch={
  .name="switch",
  .objlen=sizeof(struct ps_sprite_switch),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .configure=_ps_switch_configure,
  .get_configure_argument_name=_ps_switch_get_configure_argument_name,
  .update=_ps_switch_update,

};
