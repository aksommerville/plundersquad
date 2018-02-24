#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_EGG_AGE_CRACK 180 /* Develop a crack (tile 0x7e) */
#define PS_EGG_AGE_SPLIT 240 /* Become two tiles (0x8f), hatch a chicken, and stop participating in physics. */
#define PS_EGG_AGE_INERT 270 /* End of all activity. */

#define PS_CHICKEN_SPRDEF_ID 32
#define PS_EGG_REST_DISTANCE (PS_TILESIZE>>1) /* How far from original center are the split halves? */

/* Private sprite object.
 */

struct ps_sprite_egg {
  struct ps_sprite hdr;
  int age; // Counts updates since creation.
};

#define SPR ((struct ps_sprite_egg*)spr)

/* Delete.
 */

static void _ps_egg_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_egg_init(struct ps_sprite *spr) {
  return 0;
}

/* Stop participating in physics and hatch a chicken.
 * At some point in our lives, we all go through this.
 */

static int ps_egg_split(struct ps_sprite *spr,struct ps_game *game) {

  if (ps_game_set_group_mask_for_sprite(game,spr,
    (1<<PS_SPRGRP_KEEPALIVE)|
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
  0)<0) return -1;

  spr->layer=20;

  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_CHICKEN_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found for chicken sprite",PS_CHICKEN_SPRDEF_ID);
  } else {
    int y=spr->y-(PS_TILESIZE*3)/8;
    struct ps_sprite *chicken=ps_sprdef_instantiate(game,sprdef,0,0,spr->x,y);
    if (!chicken) return -1;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_egg_update(struct ps_sprite *spr,struct ps_game *game) {

  SPR->age++;

  switch (SPR->age) {
    case PS_EGG_AGE_SPLIT: if (ps_egg_split(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Wobble animation.
 */

static int ps_egg_wobble_for_age(int age) {
  if (age<=0) return 0;
  if (age>=PS_EGG_AGE_SPLIT) return 0;

  const int PS_EGG_WOBBLE_DISPLACEMENT=0x10;
  const int PS_EGG_WOBBLE_PERIOD=30;

  int iteration=age/PS_EGG_WOBBLE_PERIOD;
  if (!(iteration&1)) return 0;

  int iposition=age%PS_EGG_WOBBLE_PERIOD;
  double fposition=(iposition*M_PI*2.0)/PS_EGG_WOBBLE_PERIOD;
  return sin(fposition)*PS_EGG_WOBBLE_DISPLACEMENT;
}

/* Draw.
 */

static int _ps_egg_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int vtxc;
  if (SPR->age>=PS_EGG_AGE_SPLIT) vtxc=2;
  else vtxc=1;
  if (vtxc>vtxa) return vtxc;

  /* Start with the basic tile and copy to all vertices. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  int i; for (i=1;i<vtxc;i++) memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));

  /* Adjust based on age. */
  if (SPR->age>=PS_EGG_AGE_INERT) {
    vtxv[0].x-=PS_EGG_REST_DISTANCE;
    vtxv[1].x+=PS_EGG_REST_DISTANCE;
    vtxv[0].tileid+=0x10;
    vtxv[1].tileid+=0x10;
    vtxv[0].xform=AKGL_XFORM_270;
    vtxv[1].xform=AKGL_XFORM_FLOP90;

  } else if (SPR->age>=PS_EGG_AGE_SPLIT) {
    int p=SPR->age-PS_EGG_AGE_SPLIT;
    int c=PS_EGG_AGE_INERT-PS_EGG_AGE_SPLIT;
    vtxv[0].tileid+=0x10;
    vtxv[1].tileid+=0x10;
    vtxv[1].xform=AKGL_XFORM_FLOP;
    vtxv[0].t=256-(p*64)/c;
    vtxv[1].t=(p*64)/c;
    vtxv[0].x-=(p*PS_EGG_REST_DISTANCE)/c;
    vtxv[1].x+=(p*PS_EGG_REST_DISTANCE)/c;

  } else if (SPR->age>=PS_EGG_AGE_CRACK) {
    vtxv->tileid-=1;
    vtxv->t=ps_egg_wobble_for_age(SPR->age);

  } else {
    vtxv->t=ps_egg_wobble_for_age(SPR->age);

  }

  return vtxc;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_egg={
  .name="egg",
  .objlen=sizeof(struct ps_sprite_egg),

  .radius=6,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_egg_init,
  .del=_ps_egg_del,
  .update=_ps_egg_update,
  .draw=_ps_egg_draw,
  
};
