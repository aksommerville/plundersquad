/* ps_sprite_turtle.c
 * Bobs up and down in the water and permits a hero to cross on its back.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/sprites/ps_sprite_hero.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_TURTLE_FRAME_TIME   15
#define PS_TURTLE_FRAME_COUNT   2

#define PS_TURTLE_PHASE_IDLE   0
#define PS_TURTLE_PHASE_FERRY  1

#define PS_TURTLE_RIDER_EPSILON 3.0 /* Must be so close to my center to begin ride. */

#define PS_TURTLE_FERRY_SPEED 0.5

#define PS_TURTLE_RIDER_MASK ( \
  (1<<PS_SPRGRP_KEEPALIVE)| \
  (1<<PS_SPRGRP_VISIBLE)| \
  (1<<PS_SPRGRP_UPDATE)| \
  (1<<PS_SPRGRP_HERO)| \
  (1<<PS_SPRGRP_FRAGILE)| \
  (1<<PS_SPRGRP_LATCH)| \
0)

#define PS_TURTLE_IMPASSABLE_IDLE ( \
  (1<<PS_BLUEPRINT_CELL_VACANT)| \
  (1<<PS_BLUEPRINT_CELL_SOLID)| \
  (1<<PS_BLUEPRINT_CELL_LATCH)| \
  (1<<PS_BLUEPRINT_CELL_HAZARD)| \
  (1<<PS_BLUEPRINT_CELL_HEAL)| \
0)

#define PS_TURTLE_IMPASSABLE_FERRY ( \
  (1<<PS_BLUEPRINT_CELL_VACANT)| \
  (1<<PS_BLUEPRINT_CELL_SOLID)| \
  (1<<PS_BLUEPRINT_CELL_LATCH)| \
  (1<<PS_BLUEPRINT_CELL_HAZARD)| \
  (1<<PS_BLUEPRINT_CELL_HEAL)| \
  (1<<PS_BLUEPRINT_CELL_HEROONLY)| \
0)

/* Private sprite object.
 */

struct ps_sprite_turtle {
  struct ps_sprite hdr;
  struct ps_sprgrp *rider;
  int animcounter;
  int animframe;
  double dx,dy;
  int phase;
  int wait_for_disembarkment;
  uint32_t ridergrpmask;
  int riderimpassable;
};

#define SPR ((struct ps_sprite_turtle*)spr)

/* Delete.
 */

static void _ps_turtle_del(struct ps_sprite *spr) {
  ps_sprgrp_del(SPR->rider);
}

/* Initialize.
 */

static int _ps_turtle_init(struct ps_sprite *spr) {
  if (!(SPR->rider=ps_sprgrp_new())) return -1;
  SPR->phase=PS_TURTLE_PHASE_IDLE;
  return 0;
}

/* Configure.
 */

static int _ps_turtle_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_turtle_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_turtle_configure(), for editor.
  return 0;
}

/* Return all valid directions for travel, up to 4.
 */

static int ps_turtle_gather_possible_ferry_directions(int *dirv,const struct ps_sprite *spr,const struct ps_game *game) {
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col<0)||(col>=PS_GRID_COLC)) return 0;
  if ((row<0)||(row>=PS_GRID_ROWC)) return 0;
  int dirc=0;

  int west=((col>0)&&(game->grid->cellv[row*PS_GRID_COLC+col-1].physics==PS_BLUEPRINT_CELL_HOLE));
  int east=((col<PS_GRID_COLC-1)&&(game->grid->cellv[row*PS_GRID_COLC+col+1].physics==PS_BLUEPRINT_CELL_HOLE));
  int north=((row>0)&&(game->grid->cellv[(row-1)*PS_GRID_COLC+col].physics==PS_BLUEPRINT_CELL_HOLE));
  int south=((row<PS_GRID_ROWC-1)&&(game->grid->cellv[(row+1)*PS_GRID_COLC+col].physics==PS_BLUEPRINT_CELL_HOLE));

  /* First take directions where we can only go one way.
   * If we get anything here, return it.
   */
  if (west&&!east) dirv[dirc++]=PS_DIRECTION_WEST;
  else if (!west&&east) dirv[dirc++]=PS_DIRECTION_EAST;
  if (north&&!south) dirv[dirc++]=PS_DIRECTION_NORTH;
  else if (!north&&south) dirv[dirc++]=PS_DIRECTION_SOUTH;
  if (dirc) return dirc;

  /* Next, take any legal direction.
   */
  if (west) dirv[dirc++]=PS_DIRECTION_WEST;
  if (east) dirv[dirc++]=PS_DIRECTION_EAST;
  if (north) dirv[dirc++]=PS_DIRECTION_NORTH;
  if (south) dirv[dirc++]=PS_DIRECTION_SOUTH;
  return dirc;
}

/* Accept rider, enter FERRY phase.
 */

static int ps_turtle_accept_rider(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *rider) {
  if (ps_sprite_set_master(rider,spr,game)<0) return -1;
  if (ps_sprgrp_add_sprite(SPR->rider,rider)<0) return -1;
  SPR->ridergrpmask=ps_game_get_group_mask_for_sprite(game,rider);
  SPR->riderimpassable=rider->impassable;
  rider->impassable=0;
  if (ps_game_set_group_mask_for_sprite(game,rider,PS_TURTLE_RIDER_MASK)<0) return -1;
  SPR->phase=PS_TURTLE_PHASE_FERRY;
  spr->impassable=PS_TURTLE_IMPASSABLE_FERRY;
  
  int dirv[4];
  int dirc=ps_turtle_gather_possible_ferry_directions(dirv,spr,game);
  if (dirc>0) {
    int dirp=rand()%dirc;
    switch (dirv[dirp]) {
      case PS_DIRECTION_NORTH: SPR->dx=0.0; SPR->dy=-1.0; break;
      case PS_DIRECTION_SOUTH: SPR->dx=0.0; SPR->dy=1.0; break;
      case PS_DIRECTION_WEST: SPR->dx=-1.0; SPR->dy=0.0; break;
      case PS_DIRECTION_EAST: SPR->dx=1.0; SPR->dy=0.0; break;
    }
  }
  
  return 0;
}

/* Reject rider, enter IDLE phase.
 */

static int ps_turtle_reject_rider(struct ps_sprite *spr,struct ps_game *game,struct ps_sprite *rider) {
  if (rider) {
    rider->impassable=SPR->riderimpassable;
    if (ps_game_set_group_mask_for_sprite(game,rider,SPR->ridergrpmask)<0) return -1;
    if (ps_sprgrp_remove_sprite(SPR->rider,rider)<0) return -1;
    if (ps_sprite_set_master(rider,0,game)<0) return -1;
  }
  SPR->phase=PS_TURTLE_PHASE_IDLE;
  SPR->wait_for_disembarkment=1;
  spr->impassable=PS_TURTLE_IMPASSABLE_IDLE;
  
  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col>=0)&&(row>=0)&&(col<PS_GRID_COLC)&&(row<PS_GRID_ROWC)) {
    int cellp=row*PS_GRID_COLC+col;
    if (ps_game_apply_nonpersistent_grid_change(game,col,row,-1,PS_BLUEPRINT_CELL_HEROONLY,-1)<0) return -1;
    spr->x=col*PS_TILESIZE+(PS_TILESIZE>>1);
    spr->y=row*PS_TILESIZE+(PS_TILESIZE>>1);
  }
  
  return 0;
}

/* Find rider.
 */

static struct ps_sprite *ps_turtle_find_rider(const struct ps_sprite *spr,const struct ps_game *game) {
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];

    // If the hero is not currently tracking HOLE collisions, we're not interested.
    if (!(hero->impassable&(1<<PS_BLUEPRINT_CELL_HOLE))) continue;
    
    double dx=hero->x-spr->x;
    if (dx>PS_TURTLE_RIDER_EPSILON) continue;
    if (dx<-PS_TURTLE_RIDER_EPSILON) continue;
    double dy=hero->y-spr->y;
    if (dy>PS_TURTLE_RIDER_EPSILON) continue;
    if (dy<-PS_TURTLE_RIDER_EPSILON) continue;
    return hero;
  }
  return 0;
}

/* Update, IDLE phase.
 */

static int ps_turtle_update_idle(struct ps_sprite *spr,struct ps_game *game) {

  int col=spr->x/PS_TILESIZE;
  int row=spr->y/PS_TILESIZE;
  if ((col<0)||(col>=PS_GRID_COLC)) return 0;
  if ((row<0)||(row>=PS_GRID_ROWC)) return 0;
  int cellp=row*PS_GRID_COLC+col;

  switch (game->grid->cellv[cellp].physics) {
    case PS_BLUEPRINT_CELL_HOLE: { // just initialized; set this cell vacant
        if (ps_game_apply_nonpersistent_grid_change(game,col,row,-1,PS_BLUEPRINT_CELL_HEROONLY,0)<0) return -1;
      } break;
    case PS_BLUEPRINT_CELL_HEROONLY: { // wait for a rider
        struct ps_sprite *rider=ps_turtle_find_rider(spr,game);
        if (rider) {
          if (!SPR->wait_for_disembarkment) {
            if (ps_turtle_accept_rider(spr,game,rider)<0) return -1;
            if (ps_game_reverse_nonpersistent_grid_change(game,col,row)<0) return -1;
          }
        } else {
          SPR->wait_for_disembarkment=0;
        }
      } break;
  }

  return 0;
}

/* Update, FERRY phase.
 */

static int ps_turtle_update_ferry(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprite *rider=0;
  if (SPR->rider->sprc==1) rider=SPR->rider->sprv[0];

  /* Hacky fix: If the rider dies en route, we must drop it unceremoniously.
   * Don't restore anything, because the death process already overwrote our changes.
   */
  if (rider&&(rider->type==&ps_sprtype_hero)&&!((struct ps_sprite_hero*)rider)->hp) {
    ps_sprgrp_remove_sprite(SPR->rider,rider);
    rider=0;
  }

  /* Another hack: If our rider is a vampire and has turned into a bat, drop him.
   * Again, don't modify groups or impassable.
   */
  if (rider&&(rider->type==&ps_sprtype_hero)&&((struct ps_sprite_hero*)rider)->fly_in_progress) {
    if (ps_sprite_set_master(rider,0,game)<0) return -1;
    ps_sprgrp_remove_sprite(SPR->rider,rider);
    rider=0;
  }

  if (spr->collided_grid) {
    return ps_turtle_reject_rider(spr,game,rider);
  }

  spr->x+=SPR->dx*PS_TURTLE_FERRY_SPEED;
  spr->y+=SPR->dy*PS_TURTLE_FERRY_SPEED;
  if (rider) {
    rider->x=spr->x;
    rider->y=spr->y;
  }

  return 0;
}

/* Update.
 */

static int _ps_turtle_update(struct ps_sprite *spr,struct ps_game *game) {

  if (++(SPR->animcounter)>=PS_TURTLE_FRAME_TIME) {
    SPR->animcounter=0;
    if (++(SPR->animframe)>=PS_TURTLE_FRAME_COUNT) {
      SPR->animframe=0;
    }
  }

  switch (SPR->phase) {
    case PS_TURTLE_PHASE_IDLE: if (ps_turtle_update_idle(spr,game)<0) return -1; break;
    case PS_TURTLE_PHASE_FERRY: if (ps_turtle_update_ferry(spr,game)<0) return -1; break;
  }

  return 0;
}

/* Draw.
 */

static int _ps_turtle_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid+SPR->animframe;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;
  return 1;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_turtle={
  .name="turtle",
  .objlen=sizeof(struct ps_sprite_turtle),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=10,

  .init=_ps_turtle_init,
  .del=_ps_turtle_del,
  .configure=_ps_turtle_configure,
  .get_configure_argument_name=_ps_turtle_get_configure_argument_name,
  .update=_ps_turtle_update,
  .draw=_ps_turtle_draw,
  
};

/* Drop my rider, but don't change phase.
 */

int ps_sprite_turtle_drop_slave(struct ps_sprite *spr,struct ps_game *game) {
  if (!spr||(spr->type!=&ps_sprtype_turtle)) return -1;
  if (SPR->rider&&(SPR->rider->sprc>0)) {
    struct ps_sprite *rider=SPR->rider->sprv[0];
    rider->impassable=SPR->riderimpassable;
    if (ps_game_set_group_mask_for_sprite(game,rider,SPR->ridergrpmask)<0) return -1;
    if (ps_sprgrp_remove_sprite(SPR->rider,rider)<0) return -1;
  }
  return 0;
}
