#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_FLAMETHROWER_DEFAULT_LENGTH 3
#define PS_FLAMETHROWER_MAX_LENGTH     8

#define PS_FLAMETHROWER_FRAME_TIME 15
#define PS_FLAMETHROWER_FRAME_COUNT 2

#define PS_FLAMETHROWER_COLOR_ACTIVE 0xff0000
#define PS_FLAMETHROWER_COLOR_0      0x404040
#define PS_FLAMETHROWER_COLOR_1      0xffff00

/* Private sprite object.
 */

struct ps_sprite_flamethrower {
  struct ps_sprite hdr;
  int direction;
  int length;
  int animframe;
  int animtime;
  int active;
  int initial;
};

#define SPR ((struct ps_sprite_flamethrower*)spr)

/* Delete.
 */

static void _ps_flamethrower_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_flamethrower_init(struct ps_sprite *spr) {
  SPR->direction=PS_DIRECTION_NORTH;
  SPR->length=PS_FLAMETHROWER_DEFAULT_LENGTH;
  SPR->initial=1;
  return 0;
}

/* Configure.
 */

static int _ps_flamethrower_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      SPR->direction=argv[1];
      if (argc>=3) {
        SPR->length=argv[2];
      }
    }
  }
  if (!spr->switchid) SPR->active=1;
  if ((SPR->direction<1)||(SPR->direction>4)) SPR->direction=PS_DIRECTION_NORTH;
  if (SPR->length<1) SPR->length=PS_FLAMETHROWER_DEFAULT_LENGTH;
  else if (SPR->length>PS_FLAMETHROWER_MAX_LENGTH) SPR->length=PS_FLAMETHROWER_MAX_LENGTH;
  return 0;
}

static const char *_ps_flamethrower_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "(1234)=(NSWE)";
    case 2: return "length";
  }
  return 0;
}

/* First update.
 */

static int ps_flamethrower_setup(struct ps_sprite *spr,struct ps_game *game) {

  /* If the grid is flipped from its blueprint, we must flip too. Check both axes. */
  if (game&&game->scenario) {
    struct ps_screen *screen=game->scenario->screenv+game->gridy*game->scenario->w+game->gridx;
    switch (SPR->direction) {
      case PS_DIRECTION_SOUTH: if (screen->xform&PS_BLUEPRINT_XFORM_VERT) SPR->direction=PS_DIRECTION_NORTH; break;
      case PS_DIRECTION_NORTH: if (screen->xform&PS_BLUEPRINT_XFORM_VERT) SPR->direction=PS_DIRECTION_SOUTH; break;
      case PS_DIRECTION_WEST: if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) SPR->direction=PS_DIRECTION_EAST; break;
      case PS_DIRECTION_EAST: if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) SPR->direction=PS_DIRECTION_WEST; break;
    }
  }

  return 0;
}

/* Look for burnable sprites in range, and burn them.
 */

static int ps_flamethrower_burn(struct ps_sprite *spr,struct ps_game *game) {
  if (!game) return 0;
  double left=spr->x-spr->radius;
  double right=spr->x+spr->radius;
  double top=spr->y-spr->radius;
  double bottom=spr->y+spr->radius;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: top-=SPR->length*PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: bottom+=SPR->length*PS_TILESIZE; break;
    case PS_DIRECTION_WEST: left-=SPR->length*PS_TILESIZE; break;
    case PS_DIRECTION_EAST: right+=SPR->length*PS_TILESIZE; break;
  }
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->x<left) continue;
    if (victim->x>right) continue;
    if (victim->y<top) continue;
    if (victim->y>bottom) continue;
    if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_flamethrower_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->initial) {
    SPR->initial=0;
    if (ps_flamethrower_setup(spr,game)<0) return -1;
  }

  if (++(SPR->animtime)>=PS_FLAMETHROWER_FRAME_TIME) {
    SPR->animtime=0;
    if (++(SPR->animframe)>=PS_FLAMETHROWER_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  if (SPR->active) {
    if (ps_flamethrower_burn(spr,game)<0) return -1;
  }

  return 0;
}

/* Draw.
 */

static int _ps_flamethrower_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=1;
  if (SPR->active) vtxc+=SPR->length;
  if (vtxc>vtxa) return vtxc;

  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].tileid=spr->tileid;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].a=0xff;
  vtxv[0].ta=0;
  vtxv[0].t=0;

  if (SPR->active) {
    vtxv[0].pr=PS_FLAMETHROWER_COLOR_ACTIVE>>16;
    vtxv[0].pg=PS_FLAMETHROWER_COLOR_ACTIVE>>8;
    vtxv[0].pb=PS_FLAMETHROWER_COLOR_ACTIVE;
  } else if (SPR->animframe) {
    vtxv[0].pr=PS_FLAMETHROWER_COLOR_1>>16;
    vtxv[0].pg=PS_FLAMETHROWER_COLOR_1>>8;
    vtxv[0].pb=PS_FLAMETHROWER_COLOR_1;
  } else {
    vtxv[0].pr=PS_FLAMETHROWER_COLOR_0>>16;
    vtxv[0].pg=PS_FLAMETHROWER_COLOR_0>>8;
    vtxv[0].pb=PS_FLAMETHROWER_COLOR_0;
  }

  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv[0].xform=AKGL_XFORM_NONE; break;
    case PS_DIRECTION_SOUTH: vtxv[0].xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_WEST: vtxv[0].xform=AKGL_XFORM_270; break;
    case PS_DIRECTION_EAST: vtxv[0].xform=AKGL_XFORM_90; break;
  }

  if (SPR->active) {
    struct ps_vector vector=ps_vector_from_direction(SPR->direction);
    int i=1; for (;i<vtxc;i++) {
      memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
      vtxv[i].x+=i*vector.dx*PS_TILESIZE;
      vtxv[i].y+=i*vector.dy*PS_TILESIZE;
      vtxv[i].tileid-=0x10;
      if (SPR->animframe) vtxv[i].tileid++;
    }
    vtxv[vtxc-1].tileid-=0x10;
  }
  
  return vtxc;
}

/* Set switch.
 */

static int _ps_flamethrower_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  if (value) {
    if (!SPR->active) {
      SPR->active=1;
    }
  } else if (SPR->active) {
    SPR->active=0;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_flamethrower={
  .name="flamethrower",
  .objlen=sizeof(struct ps_sprite_flamethrower),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,

  .init=_ps_flamethrower_init,
  .del=_ps_flamethrower_del,
  .configure=_ps_flamethrower_configure,
  .get_configure_argument_name=_ps_flamethrower_get_configure_argument_name,
  .update=_ps_flamethrower_update,
  .draw=_ps_flamethrower_draw,
  
  .set_switch=_ps_flamethrower_set_switch,

};
