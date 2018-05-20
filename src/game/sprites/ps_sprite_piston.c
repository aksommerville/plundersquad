/* ps_sprite_piston.c
 *
 * Switch with a long piston the players can push in.
 * It then returns slowly to its original position.
 * If pressed at all, the switch is ON.
 *
 * Sprite must be placed such that only one cardinal neighbor is passable; that's where the arm goes.
 * Sprite's principal position is the control box.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_PISTON_ROLE_INIT      0
#define PS_PISTON_ROLE_BOX       1 /* master */
#define PS_PISTON_ROLE_ARM       2 /* slave */

#define PS_PISTON_RETURN_SPEED 0.5

#define PS_PISTON_LOCK_TIME       300 /* Default only. */
#define PS_PISTON_LOCK_ALERT_TIME  60
#define PS_PISTON_PREVENTLOCK_TIME 30 /* Time after unlocking when we will not re-lock. */

/* Private sprite object.
 */

struct ps_sprite_piston {
  struct ps_sprite hdr;
  int role;
  int direction; // Direction of arm from box. Players push in the opposite direction.
  double homex,homey; // Home position of arm.
  int lock; // Counts down while arm is locked to box.
  int displacement; // BOX only; set by ARM each frame.
  int enable;
  int locktime; // Constant
  int preventlock;
};

#define SPR ((struct ps_sprite_piston*)spr)

/* Delete.
 */

static void _ps_piston_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_piston_init(struct ps_sprite *spr) {
  SPR->role=PS_PISTON_ROLE_INIT;
  SPR->locktime=PS_PISTON_LOCK_TIME;
  return 0;
}

/* Configure.
 */

static int _ps_piston_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      if ((SPR->locktime=argv[1])<1) {
        SPR->locktime=PS_PISTON_LOCK_TIME;
      }
    }
  }
  return 0;
}

static const char *_ps_piston_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "locktime";
  }
  return 0;
}

/* Select direction for new piston.
 */

static int ps_piston_select_direction(const struct ps_sprite *spr,const struct ps_game *game) {

  if (!spr||!game||!game->grid) return -1;
  if ((spr->x<0.0)||(spr->y<0.0)) return -1;
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) return -1;
  
  uint8_t phn,phs,phw,phe;
  phn=(row>0)?(game->grid->cellv[(row-1)*PS_GRID_COLC+col].physics):PS_BLUEPRINT_CELL_SOLID;
  phs=(row<PS_GRID_ROWC-1)?(game->grid->cellv[(row+1)*PS_GRID_COLC+col].physics):PS_BLUEPRINT_CELL_SOLID;
  phw=(col>0)?(game->grid->cellv[row*PS_GRID_COLC+col-1].physics):PS_BLUEPRINT_CELL_SOLID;
  phe=(col<PS_GRID_COLC-1)?(game->grid->cellv[row*PS_GRID_COLC+col+1].physics):PS_BLUEPRINT_CELL_SOLID;

  #define RETURNIF(phystag) \
    if (phn==PS_BLUEPRINT_CELL_##phystag) return PS_DIRECTION_NORTH; \
    if (phs==PS_BLUEPRINT_CELL_##phystag) return PS_DIRECTION_SOUTH; \
    if (phw==PS_BLUEPRINT_CELL_##phystag) return PS_DIRECTION_WEST; \
    if (phe==PS_BLUEPRINT_CELL_##phystag) return PS_DIRECTION_EAST;
  RETURNIF(HEROONLY)
  RETURNIF(VACANT)
  RETURNIF(HOLE)
  RETURNIF(HEAL)
  RETURNIF(HAZARD)
  #undef RETURNIF

  return -1;
}

/* Allocate and attach arm.
 * We assume that we were created via sprdef, which is a safe bet and makes construction a lot easier.
 */

static struct ps_sprite *ps_piston_create_arm(struct ps_sprite *spr,struct ps_game *game) {

  int argv[]={spr->switchid,SPR->locktime};
  struct ps_sprite *arm=ps_sprdef_instantiate(game,spr->def,argv,2,spr->x,spr->y);
  if (!arm) return 0;
  struct ps_sprite_piston *ARM=(struct ps_sprite_piston*)arm;

  ARM->direction=SPR->direction;
  ARM->role=PS_PISTON_ROLE_ARM;
  arm->layer--;
  arm->tileid+=0x10;
  arm->impassable=0;

  switch (ARM->direction) {
    case PS_DIRECTION_NORTH: arm->y-=PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: arm->y+=PS_TILESIZE; break;
    case PS_DIRECTION_WEST: arm->x-=PS_TILESIZE; break;
    case PS_DIRECTION_EAST: arm->x+=PS_TILESIZE; break;
  }
  ARM->homex=arm->x;
  ARM->homey=arm->y;

  if (ps_sprite_set_master(arm,spr,game)<0) return 0;

  return arm;
}

/* First update: create arm, and set role to box.
 */

static int ps_piston_setup(struct ps_sprite *spr,struct ps_game *game) {

  if ((SPR->direction=ps_piston_select_direction(spr,game))<0) {
    ps_log(GAME,ERROR,"Failed to identify direction for piston at cell (%d,%d).",(int)(spr->x/PS_TILESIZE),(int)(spr->y/PS_TILESIZE));
    return -1;
  }

  struct ps_sprite *arm=ps_piston_create_arm(spr,game);
  if (!arm) return -1;

  SPR->role=PS_PISTON_ROLE_BOX;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_SOLID,spr)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_PHYSICS,spr)<0) return -1;
  
  return 0;
}

/* Arm position adjustment.
 */

static double ps_piston_approach(double src,double dst) {
  if (src<dst-PS_PISTON_RETURN_SPEED) return src+PS_PISTON_RETURN_SPEED;
  if (src>dst+PS_PISTON_RETURN_SPEED) return src-PS_PISTON_RETURN_SPEED;
  return dst;
}

/* Lock.
 */

static int ps_piston_arm_should_lock(const struct ps_sprite *spr,const struct ps_game *game) {
  if (SPR->preventlock) return 0;
  if (!spr->master) return 0;
  if (spr->master->sprc<1) return 0;
  struct ps_sprite *box=spr->master->sprv[0];
  double dx=box->x-spr->x;
  double dy=box->y-spr->y;
  if (dx<0.0) dx=-dx;
  if (dy<0.0) dy=-dy;
  double manhattan=dx+dy;
  if (manhattan<=2.0) return 1;
  return 0;
}

static int ps_piston_lock_arm(struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_PISTON_LOCK
  SPR->lock=SPR->locktime;
  return 0;
}

static int ps_piston_unlock_arm(struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_PISTON_UNLOCK
  SPR->lock=0;
  SPR->preventlock=PS_PISTON_PREVENTLOCK_TIME;
  return 0;
}

/* Calculate arm's displacement and deliver it to box.
 */

static int ps_piston_arm_calculate_and_deliver_displacement(struct ps_sprite *spr,struct ps_game *game) {

  if (!spr->master) return -1;
  if (spr->master->sprc<1) return -1;
  struct ps_sprite *box=spr->master->sprv[0];
  if (box->type!=&ps_sprtype_piston) return -1;
  struct ps_sprite_piston *BOX=(struct ps_sprite_piston*)box;
  
  int displacement;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: displacement=box->y-spr->y; break;
    case PS_DIRECTION_SOUTH: displacement=spr->y-box->y; break;
    case PS_DIRECTION_WEST: displacement=box->x-spr->x; break;
    case PS_DIRECTION_EAST: displacement=spr->x-box->x; break;
  }

  BOX->displacement=displacement;
  BOX->lock=SPR->lock;
  
  return 0;
}

/* Update arm.
 */

static int ps_piston_update_arm(struct ps_sprite *spr,struct ps_game *game) {
  if (SPR->preventlock>0) SPR->preventlock--;
  if (SPR->lock>0) {
    if (!--(SPR->lock)) {
      if (ps_piston_unlock_arm(spr,game)<0) return -1;
    } else if (SPR->lock==PS_PISTON_LOCK_ALERT_TIME) {
      PS_SFX_PISTON_ALERT
    }
  } else {
    spr->x=ps_piston_approach(spr->x,SPR->homex);
    spr->y=ps_piston_approach(spr->y,SPR->homey);
    if (ps_piston_arm_should_lock(spr,game)) {
      if (ps_piston_lock_arm(spr,game)<0) return -1;
    }
  }
  if (ps_piston_arm_calculate_and_deliver_displacement(spr,game)<0) return -1;
  return 0;
}

/* Update box.
 */

static int ps_piston_update_box(struct ps_sprite *spr,struct ps_game *game) {
  int enable=(SPR->displacement<PS_TILESIZE);
  if (enable&&!SPR->enable) {
    PS_SFX_PISTON_ACTIVATE
    if (ps_game_set_switch(game,spr->switchid,1)<0) return -1;
  } else if (!enable&&SPR->enable) {
    PS_SFX_PISTON_DEACTIVATE
    if (ps_game_set_switch(game,spr->switchid,0)<0) return -1;
  }
  SPR->enable=enable;
  return 0;
}

/* Update.
 */

static int _ps_piston_update(struct ps_sprite *spr,struct ps_game *game) {
  switch (SPR->role) {
    case PS_PISTON_ROLE_INIT: return ps_piston_setup(spr,game);
    case PS_PISTON_ROLE_BOX: return ps_piston_update_box(spr,game);
    case PS_PISTON_ROLE_ARM: return ps_piston_update_arm(spr,game);
  }
  return 0;
}

/* Draw.
 */

static int _ps_piston_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  /* Force position to align on the rod axis. It can sometimes get pushed a pixel or two off. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH:
    case PS_DIRECTION_SOUTH: {
        int col=spr->x/PS_TILESIZE;
        vtxv->x=col*PS_TILESIZE+(PS_TILESIZE>>1);
      } break;
    case PS_DIRECTION_WEST:
    case PS_DIRECTION_EAST: {
        int row=spr->y/PS_TILESIZE;
        vtxv->y=row*PS_TILESIZE+(PS_TILESIZE>>1);
      } break;
  }

  /* Other uncontroversial vertex setup. */
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  /* Box gets a principal color based on piston's displacement. */
  if (SPR->role==PS_PISTON_ROLE_BOX) {
    if (SPR->lock>=PS_PISTON_LOCK_ALERT_TIME) {
      vtxv->pr=0x00;
      vtxv->pg=0x80;
      vtxv->pb=0x00;
    } else if (SPR->displacement>=PS_TILESIZE) {
      vtxv->pr=0xff;
      vtxv->pg=0x00;
      vtxv->pb=0x00;
    } else if (!SPR->lock) {
      vtxv->pr=0x00;
      vtxv->pg=0xc0;
      vtxv->pb=0x00;
    } else {
      int frame=SPR->lock/8;
      if (frame&1) {
        vtxv->pr=0xff;
        vtxv->pg=0xff;
        vtxv->pb=0x00;
      } else {
        vtxv->pr=0x00;
        vtxv->pg=0x80;
        vtxv->pb=0x00;
      }
    }
  }

  /* Arm gets a transform based on direction. */
  if (SPR->role==PS_PISTON_ROLE_ARM) switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv->xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_WEST: vtxv->xform=AKGL_XFORM_90; break;
    case PS_DIRECTION_EAST: vtxv->xform=AKGL_XFORM_270; break;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_piston_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_piston={
  .name="piston",
  .objlen=sizeof(struct ps_sprite_piston),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,

  .init=_ps_piston_init,
  .del=_ps_piston_del,
  .configure=_ps_piston_configure,
  .get_configure_argument_name=_ps_piston_get_configure_argument_name,
  .update=_ps_piston_update,
  .draw=_ps_piston_draw,
  
  .set_switch=_ps_piston_set_switch,

};
