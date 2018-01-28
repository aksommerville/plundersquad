/* 4-piece rotating fireball created by the wizard.
 * "Master" sprite is required.
 */

#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_FLAMES_MOVE_SPEED 3.0
#define PS_FLAMES_ROTATE_SPEED 0.1
#define PS_FLAMES_RADIUS_GROW_SPEED 0.1
#define PS_FLAMES_RADIUS_SHRINK_SPEED 0.2
#define PS_FLAMES_TARGET_RADIUS ((PS_TILESIZE*25)/16)
#define PS_FLAMES_SCREEN_MARGIN (PS_TILESIZE*2)

/* Private sprite object.
 */

struct ps_sprite_flames {
  struct ps_sprite hdr;
  int dx,dy; // If both zero, we are tracking the master.
  double decoradius;
  double decoangle;
};

#define SPR ((struct ps_sprite_flames*)spr)

/* Delete.
 */

static void _ps_flames_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_flames_init(struct ps_sprite *spr) {

  spr->tsid=0x03;

  return 0;
}

/* Configure.
 */

static int _ps_flames_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

static const char *_ps_flames_get_configure_argument_name(int argp) {
  // Very short descriptions of arguments to _ps_flames_configure(), for editor.
  return 0;
}

/* Check for fragile sprites and kill them.
 */

static int ps_flames_check_killage(struct ps_sprite *spr,struct ps_game *game) {
  double killradius=SPR->decoradius+4.0;
  double ymin=spr->y-killradius;
  double ymax=spr->y+killradius;
  double xmin=spr->x-killradius;
  double xmax=spr->x+killradius;
  double killradius2=killradius*killradius;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_FRAGILE;
  struct ps_sprite *master=ps_sprite_get_master(spr);
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *victim=grp->sprv[i];
    
    if (victim==master) continue; // Don't kill our own master, that would be bad.

    /* First check the box because it's cheap. */
    if (victim->x<xmin) continue;
    if (victim->y<ymin) continue;
    if (victim->x>xmax) continue;
    if (victim->y>ymax) continue;

    /* Next check the square of distance -- it's only in/out, so no need to calculate the square root. */
    double dx=victim->x-spr->x;
    double dy=victim->y-spr->y;
    double qr2=dx*dx+dy*dy;
    if (qr2>killradius2) continue;

    if (ps_sprite_receive_damage(game,victim,master)<0) return -1;

  }
  return 0;
}

/* Move. 
 */

static int ps_flames_move(struct ps_sprite *spr,struct ps_game *game) {

  spr->x+=SPR->dx*PS_FLAMES_MOVE_SPEED;
  spr->y+=SPR->dy*PS_FLAMES_MOVE_SPEED;

  if (spr->x<-PS_FLAMES_SCREEN_MARGIN) return ps_sprite_kill_later(spr,game);
  if (spr->y<-PS_FLAMES_SCREEN_MARGIN) return ps_sprite_kill_later(spr,game);
  if (spr->x>PS_SCREENW+PS_FLAMES_SCREEN_MARGIN) return ps_sprite_kill_later(spr,game);
  if (spr->y>PS_SCREENH+PS_FLAMES_SCREEN_MARGIN) return ps_sprite_kill_later(spr,game);
  return 0;
}

/* Update.
 */

static int _ps_flames_update(struct ps_sprite *spr,struct ps_game *game) {

  struct ps_sprite *master=ps_sprite_get_master(spr);
  if (!master) return ps_sprite_kill_later(spr,game);
  
  SPR->decoangle+=PS_FLAMES_ROTATE_SPEED;
  if (SPR->decoangle>M_PI) SPR->decoangle-=M_PI*2.0;

  if (SPR->dx||SPR->dy) {
    SPR->decoradius-=PS_FLAMES_RADIUS_SHRINK_SPEED;
    if (SPR->decoradius<1.0) return ps_sprite_kill_later(spr,game);
    if (ps_flames_move(spr,game)<0) return -1;
    
  } else {
    if (SPR->decoradius<PS_FLAMES_TARGET_RADIUS) {
      SPR->decoradius+=PS_FLAMES_RADIUS_GROW_SPEED;
    }
    spr->x=master->x;
    spr->y=master->y;
  }

  if (ps_flames_check_killage(spr,game)<0) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_flames_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {
  if (vtxa<4) return 4;

  double dx=cos(SPR->decoangle)*SPR->decoradius;
  double dy=-sin(SPR->decoangle)*SPR->decoradius;
  vtxv->x=spr->x+dx;
  vtxv->y=spr->y+dy;
  vtxv->size=PS_TILESIZE;
  vtxv->tileid=0x6f;
  vtxv->pr=0x80;
  vtxv->pg=0x80;
  vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->ta=0;
  vtxv->t=0;
  vtxv->xform=AKGL_XFORM_NONE;

  int i; for (i=1;i<4;i++) {
    memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtxv[i].tileid+=i*0x10;
  }

  vtxv[1].x=spr->x+dy;
  vtxv[1].y=spr->y-dx;
  vtxv[2].x=spr->x-dx;
  vtxv[2].y=spr->y-dy;
  vtxv[3].x=spr->x-dy;
  vtxv[3].y=spr->y+dx;
  
  return 4;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_flames={
  .name="flames",
  .objlen=sizeof(struct ps_sprite_flames),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=120,

  .init=_ps_flames_init,
  .del=_ps_flames_del,
  .configure=_ps_flames_configure,
  .get_configure_argument_name=_ps_flames_get_configure_argument_name,
  .update=_ps_flames_update,
  .draw=_ps_flames_draw,
  
};

/* Begin moving.
 */
 
int ps_sprite_flames_throw(struct ps_sprite *spr,int direction) {
  if (!spr||(spr->type!=&ps_sprtype_flames)) return -1;
  switch (direction) {
    case PS_DIRECTION_NORTH: SPR->dy=-1; break;
    case PS_DIRECTION_SOUTH: SPR->dy=1; break;
    case PS_DIRECTION_WEST: SPR->dx=-1; break;
    case PS_DIRECTION_EAST: SPR->dx=1; break;
  }
  return 0;
}

/* Find flames by hero.
 */
 
struct ps_sprite *ps_sprite_flames_find_for_hero(const struct ps_game *game,const struct ps_sprite *hero) {
  if (!game||!hero) return 0;
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *spr=grp->sprv[i];
    if (spr->type!=&ps_sprtype_flames) continue;
    if (SPR->dx||SPR->dy) continue;
    if (ps_sprite_get_master(spr)!=hero) continue;
    return spr;
  }
  return 0;
}
