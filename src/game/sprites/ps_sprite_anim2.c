#include "ps.h"
#include "game/ps_sprite.h"

#define PS_ANIM2_FRAME_TIME_DEFAULT 15

/* Private sprite object.
 */

struct ps_sprite_anim2 {
  struct ps_sprite hdr;
  int counter;
  int frametime;
  uint8_t basetileid;
};

#define SPR ((struct ps_sprite_anim2*)spr)

/* Initialize.
 */

static int _ps_anim2_init(struct ps_sprite *spr) {
  SPR->frametime=PS_ANIM2_FRAME_TIME_DEFAULT;
  return 0;
}

/* Update.
 */

static int _ps_anim2_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->counter)>=SPR->frametime) {
    SPR->counter=0;
    if (!SPR->basetileid) SPR->basetileid=spr->tileid; // Ugly hack to get default tile.
    if (SPR->basetileid==spr->tileid) {
      spr->tileid++;
    } else {
      spr->tileid=SPR->basetileid;
    }
  }

  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_anim2={
  .name="anim2",
  .objlen=sizeof(struct ps_sprite_anim2),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_anim2_init,
  .update=_ps_anim2_update,

};
