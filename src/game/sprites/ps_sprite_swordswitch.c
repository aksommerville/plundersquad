#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"

#define PS_SWORDSWITCH_DELAY_TIME 60

/* Private sprite object.
 */

struct ps_sprite_swordswitch {
  struct ps_sprite hdr;
  int active;
};

#define SPR ((struct ps_sprite_swordswitch*)spr)

/* Configure.
 */

static int _ps_swordswitch_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
  }
  return 0;
}

static const char *_ps_swordswitch_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
  }
  return 0;
}

/* Set switch.
 */

static int _ps_swordswitch_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  if (SPR->active==value) return 0;
  if (SPR->active=value) {
    spr->tileid++;
  } else {
    spr->tileid--;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_swordswitch={
  .name="swordswitch",
  .objlen=sizeof(struct ps_sprite_swordswitch),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .configure=_ps_swordswitch_configure,
  .get_configure_argument_name=_ps_swordswitch_get_configure_argument_name,
  .set_switch=_ps_swordswitch_set_switch,

};

/* Check timer and activate if warranted.
 */
 
int ps_swordswitch_activate(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero) {
  if (!spr||(spr->type!=&ps_sprtype_swordswitch)) return -1;

  if (SPR->active^=1) {
    PS_SFX_SWORDSWITCH_UNLOCK
    if (ps_game_set_switch(game,spr->switchid,1)<0) return -1;
    spr->tileid++;

  } else {
    PS_SFX_SWORDSWITCH_LOCK
    if (ps_game_set_switch(game,spr->switchid,0)<0) return -1;
    spr->tileid--;

  }
  return 0;
}
