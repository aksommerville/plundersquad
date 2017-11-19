#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_FIREWORKS_TTL 60
#define PS_FIREWORKS_SPEED_MIN 1.5
#define PS_FIREWORKS_SPEED_MAX 3.0
#define PS_FIREWORKS_ROTATION 5

/* Private sprite object.
 */

struct ps_sprite_fireworks {
  struct ps_sprite hdr;
  double dx,dy;
  int ttl;
  struct akgl_vtx_maxtile vtx;
};

#define SPR ((struct ps_sprite_fireworks*)spr)

/* Init.
 */

static int _ps_fireworks_init(struct ps_sprite *spr) {

  spr->tsid=0x04;
  SPR->vtx.tileid=0x09;
  SPR->vtx.size=PS_TILESIZE;
  SPR->vtx.tr=SPR->vtx.tg=SPR->vtx.tb=SPR->vtx.ta=0;
  SPR->vtx.a=0xff;
  SPR->vtx.t=rand();
  SPR->vtx.xform=AKGL_XFORM_NONE;
  SPR->ttl=PS_FIREWORKS_TTL;

  return 0;
}

/* Update.
 */

static int _ps_fireworks_update(struct ps_sprite *spr,struct ps_game *game) {

  if (--(SPR->ttl)<=0) return ps_sprite_kill_later(spr,game);

  spr->x+=SPR->dx;
  spr->y+=SPR->dy;
  SPR->vtx.t+=PS_FIREWORKS_ROTATION;

  return 0;
}

/* Draw.
 */

static int _ps_fireworks_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  memcpy(vtxv,&SPR->vtx,sizeof(struct akgl_vtx_maxtile));
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->a=(SPR->ttl*0xff)/PS_FIREWORKS_TTL;
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_fireworks={
  .name="fireworks",
  .objlen=sizeof(struct ps_sprite_fireworks),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=200,

  .init=_ps_fireworks_init,
  .update=_ps_fireworks_update,
  .draw=_ps_fireworks_draw,
  
};

/* Public ctor.
 */

struct ps_sprite *ps_sprite_fireworks_new(struct ps_game *game,int x,int y,int p,int c) {
  if (!game||(p<0)||(p>=c)) return 0;

  struct ps_sprite *spr=ps_sprite_new(&ps_sprtype_fireworks);
  if (!spr) return 0;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_KEEPALIVE,spr)<0) {
    ps_sprite_del(spr);
    return 0;
  }
  ps_sprite_del(spr);
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_VISIBLE,spr)<0) return 0;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_UPDATE,spr)<0) return 0;

  spr->x=x;
  spr->y=y;

  double t=(p*M_PI*2.0)/c;
  double speed=PS_FIREWORKS_SPEED_MIN+((rand()%100)*(PS_FIREWORKS_SPEED_MAX-PS_FIREWORKS_SPEED_MIN+1))/100.0;
  SPR->dx=cos(t)*speed;
  SPR->dy=sin(t)*speed;

  SPR->vtx.pr=rand();
  SPR->vtx.pg=rand();
  SPR->vtx.pb=rand();

  return spr;
}
