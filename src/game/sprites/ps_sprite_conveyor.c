#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_CONVEYOR_FRAME_DELAY   3
#define PS_CONVEYOR_FRAME_COUNT   4
#define PS_CONVEYOR_SPEED         0.67

/* Private sprite object.
 */

struct ps_sprite_conveyor {
  struct ps_sprite hdr;
  int animtime;
  int animframe;
  int direction;
  int enable;
};

#define SPR ((struct ps_sprite_conveyor*)spr)

/* Delete.
 */

static void _ps_conveyor_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_conveyor_init(struct ps_sprite *spr) {
  SPR->direction=PS_DIRECTION_SOUTH;
  SPR->enable=1;
  return 0;
}

/* Configure.
 */

static int _ps_conveyor_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    SPR->enable=0;
    if (argc>=2) {
      SPR->direction=argv[1];
    }
  }
  return 0;
}

static const char *_ps_conveyor_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "(1234=NSWE)";
  }
  return 0;
}

/* Move pumpkins standing on me.
 */

static int ps_conveyor_convey(struct ps_sprite *spr,struct ps_game *game) {
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius-0.2;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius-0.2;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_PHYSICS;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *pumpkin=grp->sprv[i];
    if (pumpkin->x<left) continue;
    if (pumpkin->x>right) continue;
    if (pumpkin->y<top) continue;
    if (pumpkin->y>bottom) continue;
    switch (SPR->direction) {
      case PS_DIRECTION_NORTH: if ((pumpkin->y-=PS_CONVEYOR_SPEED)<top) pumpkin->y-=0.5; break;
      case PS_DIRECTION_SOUTH: if ((pumpkin->y+=PS_CONVEYOR_SPEED)>bottom) pumpkin->y+=0.5; break;
      case PS_DIRECTION_WEST: if ((pumpkin->x-=PS_CONVEYOR_SPEED)<left) pumpkin->x-=0.5; break;
      case PS_DIRECTION_EAST: if ((pumpkin->x+=PS_CONVEYOR_SPEED)>right) pumpkin->x+=0.5; break;
    }
  }
  return 0;
}

/* Update.
 */

static int _ps_conveyor_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->enable) {
    if (++(SPR->animtime)>=PS_CONVEYOR_FRAME_DELAY) {
      SPR->animtime=0;
      if (++(SPR->animframe)>=PS_CONVEYOR_FRAME_COUNT) {
        SPR->animframe=0;
      }
    }
    if (ps_conveyor_convey(spr,game)<0) return -1;
  } else {
    SPR->animtime=0;
    SPR->animframe=0;
  }

  return 0;
}

/* Draw.
 */

static int _ps_conveyor_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;

  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv->xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_SOUTH: vtxv->xform=AKGL_XFORM_NONE; break;
    case PS_DIRECTION_WEST: vtxv->xform=AKGL_XFORM_90; break;
    case PS_DIRECTION_EAST: vtxv->xform=AKGL_XFORM_270; break;
  }

  if (SPR->enable) {
    vtxv->tileid+=SPR->animframe;
  } else {
    vtxv->tileid+=0x10;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_conveyor_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  SPR->enable=value;
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_conveyor={
  .name="conveyor",
  .objlen=sizeof(struct ps_sprite_conveyor),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_conveyor_init,
  .del=_ps_conveyor_del,
  .configure=_ps_conveyor_configure,
  .get_configure_argument_name=_ps_conveyor_get_configure_argument_name,
  .update=_ps_conveyor_update,
  .draw=_ps_conveyor_draw,
  
  .set_switch=_ps_conveyor_set_switch,

};