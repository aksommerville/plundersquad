/* Short-lived visual effect to draw attention to a changed tile.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"

#define PS_CHANGEINDICATOR_TTL 40

// Rotation delta is constant:
#define PS_CHANGEINDICATOR_T_A 0
#define PS_CHANGEINDICATOR_T_Z 256

// Size goes up then down:
#define PS_CHANGEINDICATOR_SIZE_A (PS_TILESIZE<<1)
#define PS_CHANGEINDICATOR_SIZE_M (PS_TILESIZE<<1)
#define PS_CHANGEINDICATOR_SIZE_Z 1

// Color delta is constant, each channel independently:
#define PS_CHANGEINDICATOR_R_A 0xff
#define PS_CHANGEINDICATOR_G_A 0xff
#define PS_CHANGEINDICATOR_B_A 0x00
#define PS_CHANGEINDICATOR_R_Z 0x00
#define PS_CHANGEINDICATOR_G_Z 0xff
#define PS_CHANGEINDICATOR_B_Z 0x00

// Alpha is usually 100%, but ramps in and out:
#define PS_CHANGEINDICATOR_A_IN_TIME  10
#define PS_CHANGEINDICATOR_A_OUT_TIME 40

/* Private sprite object.
 */

struct ps_sprite_changeindicator {
  struct ps_sprite hdr;
  int lcp;
};

#define SPR ((struct ps_sprite_changeindicator*)spr)

/* Initialize.
 */

static int _ps_changeindicator_init(struct ps_sprite *spr) {
  spr->tsid=0x04;
  spr->tileid=0xee;
  SPR->lcp=0;
  return 0;
}

/* Update.
 */

static int _ps_changeindicator_update(struct ps_sprite *spr,struct ps_game *game) {
  SPR->lcp++;
  if (SPR->lcp<0) SPR->lcp=0;
  else if (SPR->lcp>=PS_CHANGEINDICATOR_TTL) return ps_sprite_kill_later(spr,game);
  return 0;
}

/* Draw.
 */

static int _ps_changeindicator_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  
  /* Simple fields. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->ta=0;
  vtxv->xform=AKGL_XFORM_NONE;
  
  /* Alpha. */
  if (SPR->lcp<PS_CHANGEINDICATOR_A_IN_TIME) {
    vtxv->a=(SPR->lcp*0xff)/PS_CHANGEINDICATOR_A_IN_TIME;
  } else if (SPR->lcp>PS_CHANGEINDICATOR_TTL-PS_CHANGEINDICATOR_A_OUT_TIME) {
    int p=PS_CHANGEINDICATOR_TTL-SPR->lcp;
    vtxv->a=(p*0xff)/PS_CHANGEINDICATOR_A_OUT_TIME;
  } else {
    vtxv->a=0xff;
  }
  
  /* Rotation. */
  vtxv->t=PS_CHANGEINDICATOR_T_A+(SPR->lcp*(PS_CHANGEINDICATOR_T_Z-PS_CHANGEINDICATOR_T_A-1))/PS_CHANGEINDICATOR_TTL;
  
  /* Size. */
  const int midttl=PS_CHANGEINDICATOR_TTL>>1;
  if (SPR->lcp<midttl) {
    vtxv->size=PS_CHANGEINDICATOR_SIZE_A+(SPR->lcp*(PS_CHANGEINDICATOR_SIZE_M-PS_CHANGEINDICATOR_SIZE_A+1))/midttl;
  } else {
    int p=PS_CHANGEINDICATOR_TTL-SPR->lcp;
    vtxv->size=PS_CHANGEINDICATOR_SIZE_Z+(p*(PS_CHANGEINDICATOR_SIZE_M-PS_CHANGEINDICATOR_SIZE_Z+1))/midttl;
  }
  
  /* Color. */
  vtxv->pr=PS_CHANGEINDICATOR_R_A+(SPR->lcp*(PS_CHANGEINDICATOR_R_Z-PS_CHANGEINDICATOR_R_A+1))/PS_CHANGEINDICATOR_TTL;
  vtxv->pg=PS_CHANGEINDICATOR_G_A+(SPR->lcp*(PS_CHANGEINDICATOR_G_Z-PS_CHANGEINDICATOR_G_A+1))/PS_CHANGEINDICATOR_TTL;
  vtxv->pb=PS_CHANGEINDICATOR_B_A+(SPR->lcp*(PS_CHANGEINDICATOR_B_Z-PS_CHANGEINDICATOR_B_A+1))/PS_CHANGEINDICATOR_TTL;
  
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_changeindicator={
  .name="changeindicator",
  .objlen=sizeof(struct ps_sprite_changeindicator),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=150,

  .init=_ps_changeindicator_init,
  .update=_ps_changeindicator_update,
  .draw=_ps_changeindicator_draw,

};

/* Public ctor.
 */
 
struct ps_sprite *ps_sprite_changeindicator_spawn(struct ps_game *game,int col,int row) {

  struct ps_sprite *spr=ps_sprite_new(&ps_sprtype_changeindicator);
  if (!spr) return 0;
  
  if (ps_game_set_group_mask_for_sprite(game,spr,(
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
  0))<0) {
    ps_sprite_del(spr);
    return 0;
  }
  ps_sprite_del(spr);
  
  spr->x=col*PS_TILESIZE+(PS_TILESIZE>>1);
  spr->y=row*PS_TILESIZE+(PS_TILESIZE>>1);
  
  return spr;
}
