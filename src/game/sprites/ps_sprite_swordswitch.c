#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"

#define PS_SWORDSWITCH_DELAY_TIME 60

/* Private sprite object.
 */

struct ps_sprite_swordswitch {
  struct ps_sprite hdr;
  int delay;
  int active;
  int barrierid;
};

#define SPR ((struct ps_sprite_swordswitch*)spr)

/* Configure.
 */

static int _ps_swordswitch_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc) {
  if (argc>=1) {
    SPR->barrierid=argv[0];
  }
  return 0;
}

/* Update.
 */

static int _ps_swordswitch_update(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->delay>0) SPR->delay--;
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
  .update=_ps_swordswitch_update,

};

/* Check timer and activate if warranted.
 */
 
int ps_swordswitch_activate(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *hero,int force) {
  if (!spr||(spr->type!=&ps_sprtype_swordswitch)) return -1;
  if (!force&&SPR->delay) return 0; // This is called every update. Don't strobe it.
  SPR->delay=PS_SWORDSWITCH_DELAY_TIME;

  if (SPR->active^=1) {
    if (ps_grid_open_barrier(game->grid,SPR->barrierid)<0) return -1;
    spr->tileid++;

  } else {
    if (ps_grid_close_barrier(game->grid,SPR->barrierid)<0) return -1;
    spr->tileid--;

  }
  return 0;
}
