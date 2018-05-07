#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/sprites/ps_sprite_hookshot.h" /* ps_sprite_missile_new() */
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "res/ps_resmgr.h"
#include "akgl/akgl.h"

#define PS_SHOOTER_RECOIL_TIME 20

/* Private sprite object.
 */

struct ps_sprite_shooter {
  struct ps_sprite hdr;
  int missile_sprdefid;
  int direction;
  int timing;
  int enable;
  int initial;
  int recoil; // counts down after shooting
  int clock; // counts up to (timing)
};

#define SPR ((struct ps_sprite_shooter*)spr)

/* Delete.
 */

static void _ps_shooter_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_shooter_init(struct ps_sprite *spr) {
  SPR->direction=PS_DIRECTION_SOUTH;
  SPR->enable=1;
  SPR->initial=1;
  return 0;
}

/* Configure.
 */

static int _ps_shooter_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {

  SPR->missile_sprdefid=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_missileid,SPR->missile_sprdefid);
  spr->switchid=ps_sprdef_fld_get(sprdef,PS_SPRDEF_FLD_switchid,spr->switchid);
  
  if (argc>=1) {
    SPR->direction=argv[0];
    if (argc>=2) {

      // The "timing" argument is the cycle time and also phase*1000.
      // If we forget about the phase thing, it will default to zero, which is sane.
      SPR->timing=argv[1]%1000;
      SPR->clock=argv[1]/1000;
      
      if (argc>=3) {
        SPR->missile_sprdefid=argv[2];
        if (argc>=4) {
          spr->switchid=argv[3];
          if (argc>=5) {
            SPR->enable=argv[4];
          }
        }
      }
    }
  }
  return 0;
}

static const char *_ps_shooter_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "1234 = NSWE";
    case 1: return "timing";
    case 2: return "missileid";
    case 3: return "switchid";
    case 4: return "enable";
  }
  return 0;
}

/* First update: fix orientation.
 */

static int ps_shooter_fix_orientation(struct ps_sprite *spr,struct ps_game *game) {
  if (!game) return 0;
  if (!game->scenario) return 0;
  const struct ps_screen *screen=game->scenario->screenv+game->gridy*game->scenario->w+game->gridx;
  
  if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) {
    switch (SPR->direction) {
      case PS_DIRECTION_WEST: SPR->direction=PS_DIRECTION_EAST; break;
      case PS_DIRECTION_EAST: SPR->direction=PS_DIRECTION_WEST; break;
    }
  }

  if (screen->xform&PS_BLUEPRINT_XFORM_VERT) {
    switch (SPR->direction) {
      case PS_DIRECTION_NORTH: SPR->direction=PS_DIRECTION_SOUTH; break;
      case PS_DIRECTION_SOUTH: SPR->direction=PS_DIRECTION_NORTH; break;
    }
  }

  return 0;
}

/* Fire missile.
 */

static int ps_shooter_fire(struct ps_sprite *spr,struct ps_game *game) {

  double dstx=spr->x;
  double dsty=spr->y;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: dsty-=PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: dsty+=PS_TILESIZE; break;
    case PS_DIRECTION_WEST: dstx-=PS_TILESIZE; break;
    case PS_DIRECTION_EAST: dstx+=PS_TILESIZE; break;
    default: return -1;
  }

  struct ps_sprite *missile=ps_sprite_missile_new(SPR->missile_sprdefid,spr,dstx,dsty,game);
  if (!missile) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d as shooter missile.",SPR->missile_sprdefid);
    return -1;
  }

  SPR->recoil=PS_SHOOTER_RECOIL_TIME;
  
  return 0;
}

/* Update.
 */

static int _ps_shooter_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->initial) {
    SPR->initial=0;
    if (ps_shooter_fix_orientation(spr,game)<0) return -1;
  }

  if (SPR->recoil>0) SPR->recoil--;

  if (SPR->timing>0) {
    if (++(SPR->clock)>=SPR->timing) {
      SPR->clock=0;
      if (ps_shooter_fire(spr,game)<0) return -1;
    }
  }

  return 0;
}

/* Draw.
 */

static int _ps_shooter_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;

  if (!SPR->enable) vtxv->tileid+=2;
  else if (SPR->recoil>0) vtxv->tileid+=1;
  
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv->xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_SOUTH: vtxv->xform=AKGL_XFORM_NONE; break;
    case PS_DIRECTION_WEST: vtxv->xform=AKGL_XFORM_90; break;
    case PS_DIRECTION_EAST: vtxv->xform=AKGL_XFORM_270; break;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_shooter_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  SPR->enable=value;
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_shooter={
  .name="shooter",
  .objlen=sizeof(struct ps_sprite_shooter),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_shooter_init,
  .del=_ps_shooter_del,
  .configure=_ps_shooter_configure,
  .get_configure_argument_name=_ps_shooter_get_configure_argument_name,
  .update=_ps_shooter_update,
  .draw=_ps_shooter_draw,
  
  .set_switch=_ps_shooter_set_switch,

};
