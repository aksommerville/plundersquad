#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"

/* Private sprite object.
 */

struct ps_sprite_singleswitch {
  struct ps_sprite hdr;
  int starting_value; // Constant, set at construction. My initial value.
  int fired; // If nonzero, I have already fired and am completely inert now.
  int initial; // Nonzero on first update only.
};

#define SPR ((struct ps_sprite_singleswitch*)spr)

/* Init.
 */

static int _ps_singleswitch_init(struct ps_sprite *spr) {
  SPR->initial=1;
  return 0;
}

/* Configure.
 */

static int _ps_singleswitch_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      SPR->starting_value=argv[1]?1:0;
    }
  }
  return 0;
}

static const char *_ps_singleswitch_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "initial";
  }
  return 0;
}

/* Poll for desired state.
 */

static int ps_singleswitch_poll_state(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite **presser) {
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  int i; for (i=0;i<game->grpv[PS_SPRGRP_PHYSICS].sprc;i++) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_PHYSICS].sprv[i];
    if (!(hero->impassable&(1<<PS_BLUEPRINT_CELL_HOLE))) continue; // Feet not touching ground.
    if (hero->x<=left) continue;
    if (hero->x>=right) continue;
    if (hero->y<=top) continue;
    if (hero->y>=bottom) continue;
    *presser=hero;
    return 1;
  }
  return 0;
}

/* Toggle.
 */

static int ps_singleswitch_engage(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *presser) {
  if (SPR->fired) return 0;
  SPR->fired=1;
  if (ps_game_set_switch(game,spr->switchid,SPR->starting_value^1)<0) return -1;
  if (ps_game_report_switch(game,presser)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_singleswitch_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->initial) {
    if (ps_game_set_switch(game,spr->switchid,SPR->starting_value)<0) return -1;
    SPR->initial=0;
  }

  if (SPR->fired) return 0;

  struct ps_sprite *presser=0;
  if (!ps_singleswitch_poll_state(spr,game,&presser)) return 0;

  PS_SFX_SWITCH_PRESS
  spr->tileid++;
  if (ps_singleswitch_engage(spr,game,presser)<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_singleswitch={
  .name="singleswitch",
  .objlen=sizeof(struct ps_sprite_singleswitch),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,

  .init=_ps_singleswitch_init,
  .configure=_ps_singleswitch_configure,
  .get_configure_argument_name=_ps_singleswitch_get_configure_argument_name,
  .update=_ps_singleswitch_update,

};
