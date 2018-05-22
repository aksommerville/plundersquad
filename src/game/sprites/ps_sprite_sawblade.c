#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"

#define PS_SAWBLADE_ROTATION_SPEED -5
#define PS_SAWBLADE_MOVEMENT_SPEED 3.0
#define PS_SAWBLADE_COOLOFF_TIME 60
#define PS_SAWBLADE_SCAN_DELAY 10 /* Scan for targets only so often, it's a little expensive. */

/* Private sprite object.
 */

struct ps_sprite_sawblade {
  struct ps_sprite hdr;
  uint8_t t;
  double dstx,dsty;
  int moving;
  int cooloff;
  int scandelay;
};

#define SPR ((struct ps_sprite_sawblade*)spr)

/* Delete.
 */

static void _ps_sawblade_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_sawblade_init(struct ps_sprite *spr) {
  return 0;
}

/* Configure.
 */

static int _ps_sawblade_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_sawblade_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_sawblade_configure(), for editor.
  return 0;
}

/* Have we reached the destination?
 */

static int ps_sawblade_at_destination(const struct ps_sprite *spr,const struct ps_game *game) {
  double dx=spr->x-SPR->dstx;
  if (dx<=-1.0) return 0;
  if (dx>=1.0) return 0;
  double dy=spr->y-SPR->dsty;
  if (dy<=-1.0) return 0;
  if (dy>=1.0) return 0;
  return 1;
}

/* Approach target.
 */

static double ps_sawblade_approach(double src,double dst,double d) {
  if (src<dst-d) return src+d;
  else if (src>dst+d) return src-d;
  return dst;
}

/* Look for targets in a given direction and start moving if we find one.
 * Returns >0 if found, 0 if nothing, or <0 for real errors.
 */

static int ps_sawblade_check_direction(struct ps_sprite *spr,struct ps_game *game,int col,int row,int dx,int dy) {

  /* Measure the passable grid cells in this direction. */
  int cola=col;
  int rowa=row;
  int cellc=0;
  while ((col>=0)&&(col<PS_GRID_COLC)&&(row>=0)&&(row<PS_GRID_ROWC)) {
    uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
    if (spr->impassable&(1<<physics)) break;
    col+=dx;
    row+=dy;
    cellc++;
  }
  if (!cellc) return 0;
  int colz=col-dx;
  int rowz=row-dy;
  double dstx=colz*PS_TILESIZE+(PS_TILESIZE>>1);
  double dsty=rowz*PS_TILESIZE+(PS_TILESIZE>>1);
  if (cola>colz) { int tmp=cola; cola=colz; colz=tmp; }
  if (rowa>rowz) { int tmp=rowa; rowa=rowz; rowz=tmp; }

  /* Turn that cell box into a pixel box. */
  int x=cola*PS_TILESIZE;
  int y=rowa*PS_TILESIZE;
  int w=(colz-cola+1)*PS_TILESIZE;
  int h=(rowz-rowa+1)*PS_TILESIZE;

  /* Trigger if anything fragile intersects that box. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    if (victim->x+victim->radius<x) continue;
    if (victim->x-victim->radius>x+w) continue;
    if (victim->y+victim->radius<y) continue;
    if (victim->y-victim->radius>y+h) continue;
    
    /* Got one! */
    SPR->moving=1;
    SPR->dstx=dstx;
    SPR->dsty=dsty;
    return 1;
  }
  
  return 0;
} 

/* Look for targets and start moving if appropriate.
 */

static int ps_sawblade_check_targets(struct ps_sprite *spr,struct ps_game *game) {
  if (!game||!game->grid) return 0;
  if (spr->x<0.0) return 0;
  if (spr->y<0.0) return 0;
  int col=spr->x/PS_TILESIZE;
  if (col>=PS_GRID_COLC) return 0;
  int row=spr->y/PS_TILESIZE;
  if (row>=PS_GRID_ROWC) return 0;

  /* Check all four directions and engage the first one we find something.
   * Start in a random direction, so if blockages exist during the cooloff period the players won't know which way it will go.
   */
  int startdir=rand()&3;
  int i=0; for (;i<4;i++) {
    int direction=((i+startdir)&3)+1;
    struct ps_vector vector=ps_vector_from_direction(direction);
    int err=ps_sawblade_check_direction(spr,game,col,row,vector.dx,vector.dy);
    if (err) return err;
  }
  
  return 0;
}

/* Update.
 */

static int _ps_sawblade_update(struct ps_sprite *spr,struct ps_game *game) {

  SPR->t+=PS_SAWBLADE_ROTATION_SPEED;

  if (SPR->moving) {
    spr->x=ps_sawblade_approach(spr->x,SPR->dstx,PS_SAWBLADE_MOVEMENT_SPEED);
    spr->y=ps_sawblade_approach(spr->y,SPR->dsty,PS_SAWBLADE_MOVEMENT_SPEED);
    if (ps_sawblade_at_destination(spr,game)) {
      SPR->moving=0;
      SPR->cooloff=PS_SAWBLADE_COOLOFF_TIME;
      SPR->scandelay=0;
      spr->x=SPR->dstx;
      spr->y=SPR->dsty;
    }
  } else if (SPR->cooloff>0) {
    SPR->cooloff--;
  } else if (--(SPR->scandelay)<0) {
    if (ps_sawblade_check_targets(spr,game)<0) return -1;
    SPR->scandelay=PS_SAWBLADE_SCAN_DELAY;
  }

  return 0;
}

/* Draw.
 */

static int _ps_sawblade_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<1) return 1;

  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid;
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0;
  vtxv->a=0xff;
  vtxv->t=SPR->t;
  vtxv->xform=AKGL_XFORM_NONE;
  
  return 1;
}

/* Set switch.
 */

static int _ps_sawblade_set_switch(struct ps_game *game,struct ps_sprite *spr,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_sawblade={
  .name="sawblade",
  .objlen=sizeof(struct ps_sprite_sawblade),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=100,

  .init=_ps_sawblade_init,
  .del=_ps_sawblade_del,
  .configure=_ps_sawblade_configure,
  .get_configure_argument_name=_ps_sawblade_get_configure_argument_name,
  .update=_ps_sawblade_update,
  .draw=_ps_sawblade_draw,
  
  .set_switch=_ps_sawblade_set_switch,

};
