/* ps_sprite_killozap.c
 * Two controller boxes with a bunch of laser blast lit up between them.
 * Blueprint must contain both boxes, with the same ID.
 * They must align on one axis.
 * One or the other is designated controller, then the other becomes a dummy.
 * You can then make a switch with that same ID.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_KILLOZAP_FRAME_TIME    5
#define PS_KILLOZAP_FRAME_COUNT   4
#define PS_KILLOZAP_LASER_WIDTH   6 /* *half* of the laser beam's width */

/* Private sprite object.
 */

struct ps_sprite_killozap {
  struct ps_sprite hdr;
  int barrierid;
  int controller; // Nonzero if I own the laser blast.
  int direction; // Zero initially, which flags update() to properly initialize me.
  int laserc; // Count of cells between me and partner, exclusive.
  int enable; // Nonzero if blasting (default). Controller only.
  int animframe;
  int animcounter;
};

#define SPR ((struct ps_sprite_killozap*)spr)

/* Delete.
 */

static void _ps_killozap_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_killozap_init(struct ps_sprite *spr) {
  SPR->enable=1;
  return 0;
}

/* Configure.
 */

static int _ps_killozap_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  if (argc>=1) {
    SPR->barrierid=argv[0];
  }
  return 0;
}

static const char *_ps_killozap_get_configure_argument_name(int argp) {
  switch (argp) {
    case 0: return "barrierid";
  }
  return 0;
}

/* Deferred setup.
 * The lower or leftmost of the pair becomes the controller.
 * It's important to use lower, not upper, partner -- laser is on the same layer as other sprite and should draw on top.
 * We might repeat the setup process for a given pair; that's not a problem.
 */

static int ps_killozap_setup(struct ps_sprite *spr,struct ps_game *game) {

  // Don't compare sprites' (x,y) directly because they are floating-point.
  // Instead, reduce each to the grid cell.
  int sprcol=spr->x/PS_TILESIZE;
  int sprrow=spr->y/PS_TILESIZE;

  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_VISIBLE;
  int i=grp->sprc; while (i-->0) {

    struct ps_sprite *partner=grp->sprv[i];
    if (partner->type!=&ps_sprtype_killozap) continue;
    if (partner==spr) continue;
    struct ps_sprite_killozap *PARTNER=(struct ps_sprite_killozap*)partner;
    if (PARTNER->barrierid!=SPR->barrierid) continue;
    if (PARTNER->direction) continue; // Partner already initialized, find another one.
    
    int partnercol=partner->x/PS_TILESIZE;
    int partnerrow=partner->y/PS_TILESIZE;
    if (partnercol==sprcol) { // Found partner, vertical arrangement.
      if (partnerrow==sprrow) continue; // Same cell, seriously?
      if (sprrow<partnerrow) { // spr on top
        PARTNER->direction=PS_DIRECTION_NORTH;
        PARTNER->laserc=partnerrow-sprrow-1;
        PARTNER->controller=1;
        SPR->direction=PS_DIRECTION_SOUTH;
        SPR->controller=0;
      } else { // partner on top
        SPR->direction=PS_DIRECTION_NORTH;
        SPR->laserc=sprrow-partnerrow-1;
        SPR->controller=1;
        PARTNER->direction=PS_DIRECTION_SOUTH;
        PARTNER->controller=0;
      }
      break;

    } else if (partnerrow==sprrow) { // Found partner, horizontal arrangement.
      if (partnercol==sprcol) continue; // Same cell, seriously?
      if (sprcol<partnercol) { // spr on left
        SPR->direction=PS_DIRECTION_EAST;
        SPR->laserc=partnercol-sprcol-1;
        SPR->controller=1;
        PARTNER->direction=PS_DIRECTION_WEST;
        PARTNER->controller=0;
      } else { // partner on left
        PARTNER->direction=PS_DIRECTION_EAST;
        PARTNER->laserc=sprcol-partnercol-1;
        PARTNER->controller=1;
        SPR->direction=PS_DIRECTION_WEST;
        SPR->controller=0;
      }
      break;

    }
  }

  return 0;
}

/* Look for fragile sprites inside my laser and kill them.
 */

static int ps_killozap_check_damage(struct ps_sprite *spr,struct ps_game *game) {

  /* Measure extents of the laser beam. */
  struct ps_fbox laserbox;
  switch (SPR->direction) {
    case PS_DIRECTION_SOUTH: laserbox=ps_fbox(spr->x-PS_KILLOZAP_LASER_WIDTH,spr->x+PS_KILLOZAP_LASER_WIDTH,spr->y,spr->y+PS_TILESIZE*(SPR->laserc+1)); break;
    case PS_DIRECTION_NORTH: laserbox=ps_fbox(spr->x-PS_KILLOZAP_LASER_WIDTH,spr->x+PS_KILLOZAP_LASER_WIDTH,spr->y-PS_TILESIZE*(SPR->laserc+1),spr->y); break;
    case PS_DIRECTION_WEST: laserbox=ps_fbox(spr->x-PS_TILESIZE*(SPR->laserc+1),spr->x,spr->y-PS_KILLOZAP_LASER_WIDTH,spr->y+PS_KILLOZAP_LASER_WIDTH); break;
    case PS_DIRECTION_EAST: laserbox=ps_fbox(spr->x,spr->x+PS_TILESIZE*(SPR->laserc+1),spr->y-PS_KILLOZAP_LASER_WIDTH,spr->y+PS_KILLOZAP_LASER_WIDTH); break;
    default: return 0;
  }

  /* Fry fragile sprites. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->x<laserbox.w) continue;
    if (victim->x>laserbox.e) continue;
    if (victim->y<laserbox.n) continue;
    if (victim->y>laserbox.s) continue;

    if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
  }

  /* Fry hearts. */
  grp=game->grpv+PS_SPRGRP_PRIZE;
  for (i=grp->sprc;i-->0;) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->x<laserbox.w) continue;
    if (victim->x>laserbox.e) continue;
    if (victim->y<laserbox.n) continue;
    if (victim->y>laserbox.s) continue;

    if (ps_sprite_receive_damage(game,victim,spr)<0) return -1;
  }

  return 0;
}

/* Update.
 */

static int _ps_killozap_update(struct ps_sprite *spr,struct ps_game *game) {

  /* Deferred setup. */
  if (!SPR->direction) {
    if (ps_killozap_setup(spr,game)<0) return -1;
  }

  /* Animation. */
  if (SPR->controller) {
    if (++(SPR->animcounter)>=PS_KILLOZAP_FRAME_TIME) {
      SPR->animcounter=0;
      if (++(SPR->animframe)>=PS_KILLOZAP_FRAME_COUNT) {
        SPR->animframe=0;
      }
    }
  }

  /* Hazard. */
  if (SPR->controller&&SPR->enable) {
    if (ps_killozap_check_damage(spr,game)<0) return -1;
  }
  
  return 0;
}

/* Draw.
 */

static int _ps_killozap_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  int vtxc=1;
  if (SPR->enable) vtxc+=SPR->laserc;
  if (vtxc>vtxa) return vtxc;

  vtxv[0].x=spr->x;
  vtxv[0].y=spr->y;
  vtxv[0].tileid=spr->tileid;
  vtxv[0].size=PS_TILESIZE;
  vtxv[0].ta=0;
  vtxv[0].pr=vtxv[0].pg=vtxv[0].pb=0x80;
  vtxv[0].a=0xff;
  vtxv[0].t=0;
  int dx=0,dy=0;
  switch (SPR->direction) {
    case PS_DIRECTION_NORTH: vtxv[0].xform=AKGL_XFORM_FLIP; dy=-PS_TILESIZE; break;
    case PS_DIRECTION_SOUTH: vtxv[0].xform=AKGL_XFORM_NONE; dy=PS_TILESIZE; break;
    case PS_DIRECTION_WEST: vtxv[0].xform=AKGL_XFORM_90; dx=-PS_TILESIZE; break;
    case PS_DIRECTION_EAST: vtxv[0].xform=AKGL_XFORM_270; dx=PS_TILESIZE; break;
  }

  struct akgl_vtx_maxtile *vtx=vtxv+1;
  int i=1; for (;i<vtxc;i++,vtx++) {
    memcpy(vtx,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtx->x+=dx*i;
    vtx->y+=dy*i;
    vtx->tileid=spr->tileid+1+SPR->animframe;
  }

  if (SPR->enable) {
    vtxv[0].pr=0xff; vtxv[0].pg=0x00; vtxv[0].pb=0x00;
  } else {
    vtxv[0].pr=0x00; vtxv[0].pg=0xff; vtxv[0].pb=0x00;
  }
  
  return vtxc;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_killozap={
  .name="killozap",
  .objlen=sizeof(struct ps_sprite_killozap),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_SQUARE,
  .layer=100,

  .init=_ps_killozap_init,
  .del=_ps_killozap_del,
  .configure=_ps_killozap_configure,
  .get_configure_argument_name=_ps_killozap_get_configure_argument_name,
  .update=_ps_killozap_update,
  .draw=_ps_killozap_draw,
  
};

/* Barrier state change.
 */

int ps_sprite_killozap_set_open(struct ps_sprite *spr,int open) {
  if (!spr||(spr->type!=&ps_sprtype_killozap)) return -1;
  if (open) {
    SPR->enable=0;
  } else {
    SPR->enable=1;
  }
  return 0;
}

/* Accessors.
 */
 
int ps_sprite_killozap_get_barrierid(const struct ps_sprite *spr) {
  if (!spr||(spr->type!=&ps_sprtype_killozap)) return 0;
  return SPR->barrierid;
}
