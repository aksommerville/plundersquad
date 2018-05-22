/* ps_sprite_multipiston.c
 * Advancement of ps_sprite_piston.
 * This one can have up to four arms.
 * Only when all arms are fully depressed does the switch engage.
 * For now at least, the multipiston locks once and forever (unlike regular piston).
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_sound_effects.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_MULTIPISTON_ROLE_INIT      0
#define PS_MULTIPISTON_ROLE_BOX       1
#define PS_MULTIPISTON_ROLE_ARM       2

// Pixels per arm, zero is OK.
#define PS_MULTIPISTON_DISPLACEMENT_TOLERANCE 1

#define PS_MULTIPISTON_RETURN_SPEED 0.5

/* Private sprite object.
 */

struct ps_sprite_multipiston {
  struct ps_sprite hdr;
  int role;
// box:
  int armc;
  int displacement[5]; // index is arms' (direction)
  int switch_value;
// arm:
  int direction;
  double homex,homey;
};

#define SPR ((struct ps_sprite_multipiston*)spr)

/* Delete.
 */

static void _ps_multipiston_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_multipiston_init(struct ps_sprite *spr) {
  SPR->role=PS_MULTIPISTON_ROLE_INIT;
  return 0;
}

/* Configure.
 */

static int _ps_multipiston_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    spr->switchid=argv[0];
    if (argc>=2) {
      SPR->armc=argv[1];
    }
  }
  return 0;
}

static const char *_ps_multipiston_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "switchid";
    case 1: return "armc";
  }
  return 0;
}

/* Discover neighbor availability based on grid.
 */

static int ps_multipiston_discover_neighbor_availability(int *dst,const struct ps_sprite *spr,const struct ps_game *game) {
  if (!game||!game->grid) return 0;
  if ((spr->x<0.0)||(spr->y<0.0)) return -1;
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=PS_GRID_COLC)||(row>=PS_GRID_ROWC)) return -1;

  int i=1; for (;i<=4;i++) {
    struct ps_vector vector=ps_vector_from_direction(i);
    int qcol=col+vector.dx;
    int qrow=row+vector.dy;
    if ((qcol<0)||(qcol>=PS_GRID_COLC)||(qrow<0)||(qrow>=PS_GRID_ROWC)) {
      // nope
    } else {
      uint8_t physics=game->grid->cellv[qrow*PS_GRID_COLC+qcol].physics;
      switch (physics) {
        case PS_BLUEPRINT_CELL_HEROONLY:
        case PS_BLUEPRINT_CELL_VACANT:
        case PS_BLUEPRINT_CELL_HOLE:
        case PS_BLUEPRINT_CELL_HEAL:
        case PS_BLUEPRINT_CELL_HAZARD: {
            dst[i]=1;
          }
      }
    }
  }
  
  return 0;
}

/* Spawn one arm during setup.
 */

static struct ps_sprite *ps_multipiston_spawn_arm(struct ps_sprite *box,struct ps_game *game,int direction) {
  struct ps_sprite_multipiston *BOX=(struct ps_sprite_multipiston*)box;

  int argv[]={box->switchid,BOX->armc};
  struct ps_sprite *arm=ps_sprdef_instantiate(game,box->def,argv,2,box->x,box->y);
  if (!arm) return 0;
  struct ps_sprite_multipiston *ARM=(struct ps_sprite_multipiston*)arm;

  ARM->direction=direction;
  ARM->role=PS_MULTIPISTON_ROLE_ARM;
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

  if (ps_sprite_set_master(arm,box,game)<0) return 0;

  return arm;
}

/* Initial setup (box).
 */

static int ps_multipiston_setup(struct ps_sprite *spr,struct ps_game *game) {

  /* Validate arm count against hard limits and our neighbor cells.
   */
  int neighbor_availability[5]={0}; // indexed by direction, 0 is a dummy
  if (ps_multipiston_discover_neighbor_availability(neighbor_availability,spr,game)<0) return -1;
  int availarmc=neighbor_availability[1]+neighbor_availability[2]+neighbor_availability[3]+neighbor_availability[4];
  if ((SPR->armc<1)||(SPR->armc>availarmc)) {
    ps_log(GAME,ERROR,
      "multipiston requested %d arms; grid has %d space%s available",
      SPR->armc,availarmc,(availarmc==1)?"":"s"
    );
    return -1;
  }

  /* Create arms.
   * Don't bother with any direction-selection logic; grid should have the exact count of available slots.
   * And if it's more, it's not really a big deal.
   */
  int armc=SPR->armc;
  int i=1; for (;i<=4;i++) {
    if (!neighbor_availability[i]) continue;
    if (!ps_multipiston_spawn_arm(spr,game,i)) return -1;
    SPR->displacement[i]=99; // Ensure we do not switch on at our first update.
    if (!--armc) break;
  }

  /* Setup the box. */
  SPR->role=PS_MULTIPISTON_ROLE_BOX;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_SOLID,spr)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_PHYSICS,spr)<0) return -1;
  
  return 0;
}

/* Test activation.
 */

static int ps_multipiston_box_should_activate(const struct ps_sprite *spr,const struct ps_game *game) {
  int i=1;
  for (;i<=4;i++) {
    if (SPR->displacement[i]>PS_MULTIPISTON_DISPLACEMENT_TOLERANCE) return 0;
  }
  return 1;
}

/* Update box.
 */

static int ps_multipiston_update_box(struct ps_sprite *spr,struct ps_game *game) {

  if (!SPR->switch_value) {
    if (ps_multipiston_box_should_activate(spr,game)) {
      PS_SFX_PISTON_LOCK // LOCK or ACTIVATE, or a new sound?
      if (ps_game_set_switch(game,spr->switchid,1)<0) return -1;
      SPR->switch_value=1;
    }
  }
  
  return 0;
}

/* Arm position adjustment.
 */

static double ps_multipiston_approach(double src,double dst) {
  if (src<dst-PS_MULTIPISTON_RETURN_SPEED) return src+PS_MULTIPISTON_RETURN_SPEED;
  if (src>dst+PS_MULTIPISTON_RETURN_SPEED) return src-PS_MULTIPISTON_RETURN_SPEED;
  return dst;
}

/* Update arm.
 */

static int ps_multipiston_update_arm(struct ps_sprite *spr,struct ps_game *game) {

  /* If we're active, lock to the box's position and return. */
  if (SPR->switch_value) {
    if (spr->master&&(spr->master->sprc>=1)) {
      //ps_log(GAME,DEBUG,"*** multipiston arm %p switched on ***",spr);
      struct ps_sprite *box=spr->master->sprv[0];
      spr->x=box->x;
      spr->y=box->y;
    }
    return 0;
  }

  /* Inactive, return to home position. */
  spr->x=ps_multipiston_approach(spr->x,SPR->homex);
  spr->y=ps_multipiston_approach(spr->y,SPR->homey);

  /* Update master's displacement record. */
  if (spr->master&&(spr->master->sprc>=1)) {
    struct ps_sprite *box=spr->master->sprv[0];
    if (box->type==&ps_sprtype_multipiston) {
      struct ps_sprite_multipiston *BOX=(struct ps_sprite_multipiston*)box;
      
      int displacement;
      switch (SPR->direction) {
        case PS_DIRECTION_NORTH: displacement=box->y-spr->y; break;
        case PS_DIRECTION_SOUTH: displacement=spr->y-box->y; break;
        case PS_DIRECTION_WEST: displacement=box->x-spr->x; break;
        case PS_DIRECTION_EAST: displacement=spr->x-box->x; break;
        default: return -1;
      }

      BOX->displacement[SPR->direction]=displacement;
    }
  }

  return 0;
}

/* Update.
 */

static int _ps_multipiston_update(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->role==PS_MULTIPISTON_ROLE_INIT) {
    if (ps_multipiston_setup(spr,game)<0) return -1;
  }

  if (SPR->role==PS_MULTIPISTON_ROLE_BOX) {
    return ps_multipiston_update_box(spr,game);
  } else if (SPR->role==PS_MULTIPISTON_ROLE_ARM) {
    return ps_multipiston_update_arm(spr,game);
  }
  return 0;
}

/* Draw.
 */

static int _ps_multipiston_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
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

  /* Box gets a principal color based value.
   */
  if (SPR->role==PS_MULTIPISTON_ROLE_BOX) {
    if (SPR->switch_value) {
      vtxv->pr=0x00;
      vtxv->pg=0x80;
      vtxv->pb=0x00;
    } else {
      vtxv->pr=0xff;
      vtxv->pg=0x00;
      vtxv->pb=0x00;
    }
  }

  /* Arm gets a transform based on direction. */
  if (SPR->role==PS_MULTIPISTON_ROLE_ARM) switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv->xform=AKGL_XFORM_FLIP; break;
    case PS_DIRECTION_WEST: vtxv->xform=AKGL_XFORM_90; break;
    case PS_DIRECTION_EAST: vtxv->xform=AKGL_XFORM_270; break;
  }
  
  return 1;
}

/* Set switch.
 */

static int _ps_multipiston_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  SPR->switch_value=value;
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_multipiston={
  .name="multipiston",
  .objlen=sizeof(struct ps_sprite_multipiston),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,
  .ignore_collisions_on_same_type=1,

  .init=_ps_multipiston_init,
  .del=_ps_multipiston_del,
  .configure=_ps_multipiston_configure,
  .get_configure_argument_name=_ps_multipiston_get_configure_argument_name,
  .update=_ps_multipiston_update,
  .draw=_ps_multipiston_draw,
  
  .set_switch=_ps_multipiston_set_switch,

};
