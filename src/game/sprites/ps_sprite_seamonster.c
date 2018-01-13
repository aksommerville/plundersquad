#include "ps.h"
#include "game/ps_sprite.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"
#include "game/sprites/ps_sprite_hookshot.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "akgl/akgl.h"
#include <math.h>

#define PS_SEAMONSTER_PHASE_RESET       0 /* Default, prepare the next swim phase. */
#define PS_SEAMONSTER_PHASE_SWIM        1 /* Head down, no tentacles, moving about in the water. */
#define PS_SEAMONSTER_PHASE_LURK        2 /* Head down, no tentacles, waiting ominously. */
#define PS_SEAMONSTER_PHASE_PREFIRE     3 /* Head up, tentacles out, waiting to spit a fireball. */
#define PS_SEAMONSTER_PHASE_FIRE        4 /* Head up, mouth open, fireball issues at the start of this phase. */
#define PS_SEAMONSTER_PHASE_POSTFIRE    5 /* Same as PREFIRE, after shooting. */

#define PS_SEAMONSTER_SWIM_DISTANCE_MIN (PS_TILESIZE)
#define PS_SEAMONSTER_SWIM_DISTANCE_MAX (PS_TILESIZE*5)
#define PS_SEAMONSTER_SWIM_SPEED 1.0

#define PS_SEAMONSTER_FRAME_TIME 10 /* Cycles per frame of all three animated faces. */

#define PS_SEAMONSTER_LURK_TIME_MIN      20
#define PS_SEAMONSTER_LURK_TIME_MAX     140
#define PS_SEAMONSTER_PREFIRE_TIME_MIN   20
#define PS_SEAMONSTER_PREFIRE_TIME_MAX   40
#define PS_SEAMONSTER_POSTFIRE_TIME_MIN  20
#define PS_SEAMONSTER_POSTFIRE_TIME_MAX  50
#define PS_SEAMONSTER_FIRE_TIME          25
#define PS_SEAMONSTER_SWIM_TIME_MAX     240 /* Time limit, if he doesn't reach target. */

#define PS_SEAMONSTER_TENTACLE_LIMIT         6
#define PS_SEAMONSTER_TENTACLE_DISTANCE      ((PS_TILESIZE*85.0)/100.0)
#define PS_SEAMONSTER_TENTACLE_FLATTEN_VERT  0.70 /* Make tentacle spread slightly oblong. */
#define PS_SEAMONSTER_TENTACLE_RADIUS        3

#define PS_SEAMONSTER_FIREBALL_SPRDEF_ID 11

#define PS_SEAMONSTER_FACING_RIGHT (spr->x<(PS_SCREENW>>1))

/* Private sprite object.
 */

struct ps_tentacle {
  double x,y;
  uint8_t xform; // Either AKGL_XFORM_NONE (left) or AKGL_XFORM_FLOP (right)
};

struct ps_sprite_seamonster {
  struct ps_sprite hdr;
  int phase;
  int phasetimer; // Counts down to next phase.
  int animtimer; // Counts up to PS_SEAMONSTER_FRAME_TIME
  int animframe; // 0 or 1. All faces have either 1 or 2 frames, so we update this clock generically.
  double dstx,dsty; // Target position in SWIM phase.
  double dx,dy; // Calculated movement vector in SWIM phase, apply directly.
  struct ps_tentacle tentaclev[PS_SEAMONSTER_TENTACLE_LIMIT];
  int tentaclec;
  int holec; // Count of HOLE tiles in grid; refresh if zero.
  int first_update; // To prevent DIVE sound from all sea monsters at screen start.
};

#define SPR ((struct ps_sprite_seamonster*)spr)

/* Delete.
 */

static void _ps_seamonster_del(struct ps_sprite *spr) {
}

/* Initialize.
 */

static int _ps_seamonster_init(struct ps_sprite *spr) {

  SPR->phase=PS_SEAMONSTER_PHASE_RESET;
  SPR->first_update=1;
  
  return 0;
}

/* Configure.
 */

static int _ps_seamonster_configure(struct ps_sprite *spr,struct ps_game *game,const int *argv,int argc,const struct ps_sprdef *sprdef) {
  return 0;
}

/* Count holes in grid, or select one by index.
 */

static int ps_seamonster_count_grid_holes(const struct ps_grid *grid) {
  if (!grid) return 0;
  int c=0,i=PS_GRID_SIZE;
  const struct ps_grid_cell *cell=grid->cellv;
  for (;i-->0;cell++) {
    if (cell->physics==PS_BLUEPRINT_CELL_HOLE) c++;
  }
  return c;
}

static int ps_seamonster_get_indexed_hole_cell(int *col,int *row,const struct ps_grid *grid,int holep) {
  if (!grid) return -1;
  int colp=0,rowp=0;
  const struct ps_grid_cell *cell=grid->cellv;
  for (;rowp<PS_GRID_ROWC;rowp++) {
    for (colp=0;colp<PS_GRID_COLC;colp++,cell++) {
      if (cell->physics==PS_BLUEPRINT_CELL_HOLE) {
        if (!holep--) {
          *col=colp;
          *row=rowp;
          return 0;
        }
      }
    }
  }
  return -1;
}

/* Set destination for swimming.
 */

static int ps_seamonster_select_swim_dst(struct ps_sprite *spr,struct ps_game *game) {

  if (!SPR->holec) {
    SPR->holec=ps_seamonster_count_grid_holes(game->grid);
  }

  if (SPR->holec>0) {
    int holep=rand()%SPR->holec;
    int col,row;
    if (ps_seamonster_get_indexed_hole_cell(&col,&row,game->grid,holep)<0) return -1;
    SPR->dstx=col*PS_TILESIZE+(PS_TILESIZE>>1);
    SPR->dsty=row*PS_TILESIZE+(PS_TILESIZE>>1);
  } else {
    ps_log(GAME,ERROR,"Sea monster in a grid with no HOLE cells.");
    return -1;
  }

  double dx=SPR->dstx-spr->x;
  double dy=SPR->dsty-spr->y;
  double distance=sqrt(dx*dx+dy*dy);
  if (distance>0.0) {
    SPR->dx=(dx*PS_SEAMONSTER_SWIM_SPEED)/distance;
    SPR->dy=(dy*PS_SEAMONSTER_SWIM_SPEED)/distance;
  } else {
    SPR->dx=0.0;
    SPR->dy=0.0;
  }
  
  return 0;
}

/* Examine closely one possible grid collision and correct position if needed.
 */

static int ps_seamonster_collide_cell(struct ps_sprite *spr,struct ps_game *game,int col,int row) {
  struct ps_fbox cellbox=ps_fbox(col*PS_TILESIZE,(col+1)*PS_TILESIZE,row*PS_TILESIZE,(row+1)*PS_TILESIZE);
  struct ps_circle sprcircle=ps_circle(spr->x,spr->y,spr->radius);
  struct ps_overlap overlap;
  if (!ps_fbox_collide_circle(&overlap,cellbox,sprcircle)) return 0;
  spr->x+=overlap.axis.dx*overlap.penetration;
  spr->y+=overlap.axis.dy*overlap.penetration;
  return 1;
}

/* Consider position and force into the water if possible.
 */

static int ps_seamonster_correct_position(struct ps_sprite *spr,struct ps_game *game) {

  if (!game->grid) return 0;

  int xa=spr->x-spr->radius;
  int xz=spr->x+spr->radius;
  int ya=spr->y-spr->radius;
  int yz=spr->y+spr->radius;
  int cola=xa/PS_TILESIZE; if (cola<0) cola=0; else if (cola>=PS_GRID_COLC) cola=PS_GRID_COLC-1;
  int colz=xz/PS_TILESIZE; if (colz<0) colz=0; else if (colz>=PS_GRID_COLC) colz=PS_GRID_COLC-1;
  int rowa=ya/PS_TILESIZE; if (rowa<0) rowa=0; else if (rowa>=PS_GRID_ROWC) rowa=PS_GRID_ROWC-1;
  int rowz=yz/PS_TILESIZE; if (rowz<0) rowz=0; else if (rowz>=PS_GRID_ROWC) rowz=PS_GRID_ROWC-1;

  const struct ps_grid_cell *cellrow=game->grid->cellv+PS_GRID_COLC*rowa;
  int rowp=rowa; for (;rowp<=rowz;rowp++,cellrow+=PS_GRID_COLC) {
    int colp=cola; for (;colp<=colz;colp++) {
      if (cellrow[colp].physics!=PS_BLUEPRINT_CELL_HOLE) {
        if (ps_seamonster_collide_cell(spr,game,colp,rowp)<0) return -1;
      }
    }
  }
  
  return 0;
}

/* Update swimming.
 */

static int ps_seamonster_approach_1axis(double *position,double destination,double *delta) {
  if (*position==destination) return 1;
  if (*position<destination) {
    if (*delta<0.0) {
      *position=destination;
      *delta=0.0;
      return 1;
    }
  } else {
    if (*delta>0.0) {
      *position=destination;
      *delta=0.0;
      return 1;
    }
  }
  (*position)+=*delta;
  return 0;
}

static int ps_seamonster_update_swim(struct ps_sprite *spr,struct ps_game *game) {
  int donex=ps_seamonster_approach_1axis(&spr->x,SPR->dstx,&SPR->dx);
  int doney=ps_seamonster_approach_1axis(&spr->y,SPR->dsty,&SPR->dy);
  if (donex&&doney) {
    SPR->phasetimer=0;
    return 0;
  }
  if (ps_seamonster_correct_position(spr,game)<0) return -1;
  return 0;
}

/* Calculate and store tentacle positions.
 */

static int ps_seamonster_ok_tentacle_position(int x,int y,struct ps_grid *grid) {
  if (!grid) return 1;

  int xa=x-PS_SEAMONSTER_TENTACLE_RADIUS;
  int xz=x+PS_SEAMONSTER_TENTACLE_RADIUS;
  int ya=y-PS_SEAMONSTER_TENTACLE_RADIUS;
  int yz=y+PS_SEAMONSTER_TENTACLE_RADIUS;
  int cola=xa/PS_TILESIZE; if (cola<0) cola=0; else if (cola>=PS_GRID_COLC) cola=PS_GRID_COLC-1;
  int colz=xz/PS_TILESIZE; if (colz<0) colz=0; else if (colz>=PS_GRID_COLC) colz=PS_GRID_COLC-1;
  int rowa=ya/PS_TILESIZE; if (rowa<0) rowa=0; else if (rowa>=PS_GRID_ROWC) rowa=PS_GRID_ROWC-1;
  int rowz=yz/PS_TILESIZE; if (rowz<0) rowz=0; else if (rowz>=PS_GRID_ROWC) rowz=PS_GRID_ROWC-1;

  const struct ps_grid_cell *cellrow=grid->cellv+PS_GRID_COLC*rowa;
  int rowp=rowa; for (;rowp<=rowz;rowp++,cellrow+=PS_GRID_COLC) {
    int colp=cola; for (;colp<=colz;colp++) {
      if (cellrow[colp].physics!=PS_BLUEPRINT_CELL_HOLE) {
        return 0;
      }
    }
  }
  
  return 1;
}

static int ps_seamonster_initialize_tentacle(struct ps_tentacle *tentacle,struct ps_sprite *spr,struct ps_game *game,double t) {
  tentacle->x=spr->x+cos(t)*PS_SEAMONSTER_TENTACLE_DISTANCE;
  tentacle->y=spr->y-(sin(t)*PS_SEAMONSTER_TENTACLE_DISTANCE*PS_SEAMONSTER_TENTACLE_FLATTEN_VERT);
  if (!ps_seamonster_ok_tentacle_position(tentacle->x,tentacle->y,game->grid)) return 0;
  if (tentacle->x<spr->x) {
    tentacle->xform=AKGL_XFORM_NONE;
  } else {
    tentacle->xform=AKGL_XFORM_FLOP;
  }
  return 1;
}

static void ps_seamonster_sort_tentacles_vertically(struct ps_tentacle *v,int c) {
  int lo=0,hi=c-1,d=1;
  while (lo<hi) {
    int first,last,i,done=1;
    if (d==1) { first=lo; last=hi; }
    else { first=hi; last=lo; }
    for (i=first;i!=last;i+=d) {
      int cmp=(v[i].y<v[i+d].y)?-1:1;
      if (cmp==d) {
        struct ps_tentacle tmp=v[i];
        v[i]=v[i+d];
        v[i+d]=tmp;
        done=0;
      }
    }
    if (done) return;
    if (d==1) { d=-1; hi--; }
    else { d=1; lo++; }
  }
}

static int ps_seamonster_calculate_tentacle_positions(struct ps_sprite *spr,struct ps_game *game) {
  SPR->tentaclec=0;
  double t0=(rand()%100)/100.0; // Start randomly within about an eighth turn.
  int i=0; for (;i<PS_SEAMONSTER_TENTACLE_LIMIT;i++) {
    double t=t0+(i*M_PI*2.0)/PS_SEAMONSTER_TENTACLE_LIMIT;
    if (ps_seamonster_initialize_tentacle(SPR->tentaclev+SPR->tentaclec,spr,game,t)>0) {
      SPR->tentaclec++;
    }
  }
  ps_seamonster_sort_tentacles_vertically(SPR->tentaclev,SPR->tentaclec);
  return 0;
}

/* Decide where the fireball should go and create it.
 * We will consider the direction the sea monster is facing (left or right, depending which half of screen we are on).
 * Then consider a sector 90 degrees wide pointing that direction.
 * If there's any heroes in the Cone of Potential Death, pick one at random.
 * Otherwise, pick a random angle.
 * Axis-aligned 90-degree sectors happen to present a delightful optimization, read on...
 */

static struct ps_sprite *ps_seamonster_select_target(struct ps_sprite *spr,struct ps_game *game) {
  if (!game) return 0;
  if (game->grpv[PS_SPRGRP_HERO].sprc<1) return 0;

  double dx; // Which way I am facing.
  if (PS_SEAMONSTER_FACING_RIGHT) dx=1.0; else dx=-1.0;
  
  struct ps_sprite *herov[PS_PLAYER_LIMIT];
  int heroc=0,i;
  for (i=game->grpv[PS_SPRGRP_HERO].sprc;i-->0;) {
    struct ps_sprite *candidate=game->grpv[PS_SPRGRP_HERO].sprv[i];
    double candx=candidate->x-spr->x;
    if (((candx<0.0)&&(dx>0.0))||((candx>0.0)&&(dx<0.0))) continue; // behind my back.
    double candy=candidate->y-spr->y;
    if (candx<0.0) candx=-candx;
    if (candy<0.0) candy=-candy;
    if (candy>candx) continue; // Ooh, it's so easy when everything is axis-aligned!
    herov[heroc++]=candidate;
    if (heroc>=PS_PLAYER_LIMIT) break; // Too many players, whatever, not my problem.
  }

  if (heroc<1) return 0;
  if (heroc==1) return herov[0];
  return herov[rand()%heroc];
}

static int ps_seamonster_select_default_fireball_target(double *dstx,double *dsty,struct ps_sprite *spr,struct ps_game *game) {
  // Offset (*dstx) by a constant amount, then (*dsty) in either direction up to that same constant, randomly.
  if (PS_SEAMONSTER_FACING_RIGHT) *dstx=spr->x+100.0; else *dstx=spr->x-100.0;
  *dsty=spr->y-100.0+rand()%200;
  return 0;
}

static int ps_seamonster_shoot_fireball(struct ps_sprite *spr,struct ps_game *game) {

  double dstx,dsty;
  struct ps_sprite *target=ps_seamonster_select_target(spr,game);
  if (!target) { // No victim in range or whatever. No big deal, we have a fallback.
    if (ps_seamonster_select_default_fireball_target(&dstx,&dsty,spr,game)<0) return -1;
  } else {
    dstx=target->x;
    dsty=target->y;
  }

  PS_SFX_SEAMONSTER_FIRE
  
  struct ps_sprite *fireball=ps_sprite_missile_new(PS_SEAMONSTER_FIREBALL_SPRDEF_ID,spr,dstx,dsty,game);
  if (!fireball) return -1;
  
  return 0;
}

/* Begin the phases.
 * These must set SPR->phase and SPR->phasetimer, and call any other init hooks.
 */

static int ps_seamonster_begin_swim(struct ps_sprite *spr,struct ps_game *game) {
  if (!SPR->first_update) {
    PS_SFX_SEAMONSTER_DIVE
  }
  SPR->phase=PS_SEAMONSTER_PHASE_SWIM;
  SPR->phasetimer=PS_SEAMONSTER_SWIM_TIME_MAX;
  SPR->tentaclec=0;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
  if (ps_sprgrp_remove_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,spr)<0) return -1;
  if (ps_seamonster_select_swim_dst(spr,game)<0) return -1;
  return 0;
}

static int ps_seamonster_begin_lurk(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_SEAMONSTER_PHASE_LURK;
  SPR->phasetimer=PS_SEAMONSTER_LURK_TIME_MIN+rand()%(PS_SEAMONSTER_LURK_TIME_MAX-PS_SEAMONSTER_LURK_TIME_MIN+1);
  return 0;
}

static int ps_seamonster_begin_prefire(struct ps_sprite *spr,struct ps_game *game) {
  PS_SFX_SEAMONSTER_SURFACE
  SPR->phase=PS_SEAMONSTER_PHASE_PREFIRE;
  SPR->phasetimer=PS_SEAMONSTER_PREFIRE_TIME_MIN+rand()%(PS_SEAMONSTER_PREFIRE_TIME_MAX-PS_SEAMONSTER_PREFIRE_TIME_MIN+1);
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_FRAGILE,spr)<0) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,spr)<0) return -1;
  if (ps_seamonster_calculate_tentacle_positions(spr,game)<0) return -1;
  return 0;
}

static int ps_seamonster_begin_fire(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_SEAMONSTER_PHASE_FIRE;
  SPR->phasetimer=PS_SEAMONSTER_FIRE_TIME;
  if (ps_seamonster_shoot_fireball(spr,game)<0) return -1;
  return 0;
}

static int ps_seamonster_begin_postfire(struct ps_sprite *spr,struct ps_game *game) {
  SPR->phase=PS_SEAMONSTER_PHASE_POSTFIRE;
  SPR->phasetimer=PS_SEAMONSTER_POSTFIRE_TIME_MIN+rand()%(PS_SEAMONSTER_POSTFIRE_TIME_MAX-PS_SEAMONSTER_POSTFIRE_TIME_MIN+1);
  return 0;
}

/* Update phase selection.
 */

static int ps_seamonster_update_phase_selection(struct ps_sprite *spr,struct ps_game *game) {

  if (SPR->phase==PS_SEAMONSTER_PHASE_RESET) SPR->phasetimer=0;
  else SPR->phasetimer--;

  if (SPR->phasetimer<=0) switch (SPR->phase) {
    case PS_SEAMONSTER_PHASE_RESET: if (ps_seamonster_begin_swim(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_SWIM: if (ps_seamonster_begin_lurk(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_LURK: if (ps_seamonster_begin_prefire(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_PREFIRE: if (ps_seamonster_begin_fire(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_FIRE: if (ps_seamonster_begin_postfire(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_POSTFIRE: if (ps_seamonster_begin_swim(spr,game)<0) return -1; break;
    default: if (ps_seamonster_begin_swim(spr,game)<0) return -1;
  }

  return 0;
}

/* Update animation.
 */

static int ps_seamonster_update_animation(struct ps_sprite *spr) {
  SPR->animtimer++;
  if (SPR->animtimer>=PS_SEAMONSTER_FRAME_TIME) {
    SPR->animtimer=0;
    SPR->animframe^=1;
  }
  return 0;
}

/* Update.
 */

static int _ps_seamonster_update(struct ps_sprite *spr,struct ps_game *game) {
  if (ps_seamonster_update_animation(spr)<0) return -1;
  if (ps_seamonster_update_phase_selection(spr,game)<0) return -1;
  switch (SPR->phase) {
    case PS_SEAMONSTER_PHASE_RESET: if (ps_seamonster_begin_swim(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_SWIM: if (ps_seamonster_update_swim(spr,game)<0) return -1; break;
    case PS_SEAMONSTER_PHASE_LURK: break;
    case PS_SEAMONSTER_PHASE_PREFIRE: break;
    case PS_SEAMONSTER_PHASE_FIRE: break;
    case PS_SEAMONSTER_PHASE_POSTFIRE: break;
  }
  SPR->first_update=0;
  return 0;
}

/* Draw.
 */

static int _ps_seamonster_draw(struct akgl_vtx_maxtile *vtxv,int vtxa,struct ps_sprite *spr) {

  int vtxc=1+SPR->tentaclec;
  if (vtxc>vtxa) return vtxc;

  /* Set the base vertex with sensible defaults. */
  vtxv->x=spr->x;
  vtxv->y=spr->y;
  vtxv->tileid=spr->tileid; // Base tile, will update later based on phase and animation.
  vtxv->size=PS_TILESIZE;
  vtxv->ta=0x00;
  vtxv->pr=vtxv->pg=vtxv->pb=0x80;
  vtxv->a=0xff;
  vtxv->t=0;
  vtxv->xform=(PS_SEAMONSTER_FACING_RIGHT?AKGL_XFORM_FLOP:AKGL_XFORM_NONE); // Face center.

  /* If we have tentacles, copy the base vertex then update (x,y,tileid,xform). 
   * First vertex must still be on the base tile at this point.
   */
  const struct ps_tentacle *tentacle=SPR->tentaclev;
  int i=1; for (;i<vtxc;i++,tentacle++) {
    memcpy(vtxv+i,vtxv,sizeof(struct akgl_vtx_maxtile));
    vtxv[i].x=tentacle->x;
    vtxv[i].y=tentacle->y;
    vtxv[i].xform=tentacle->xform;
    vtxv[i].tileid+=(SPR->animframe?0x11:0x10);
  }

  /* Update base frame. */
  switch (SPR->phase) {
    case PS_SEAMONSTER_PHASE_PREFIRE:
    case PS_SEAMONSTER_PHASE_POSTFIRE: {
        if (SPR->animframe) vtxv->tileid+=0x01;
        else vtxv->tileid+=0x00;
      } break;
    case PS_SEAMONSTER_PHASE_FIRE: {
        vtxv->tileid+=0x02;
      } break;
    case PS_SEAMONSTER_PHASE_SWIM: 
    case PS_SEAMONSTER_PHASE_LURK: {
        if (SPR->animframe) vtxv->tileid+=0x03;
        else vtxv->tileid+=0x04;
      } break;
  }

  /* One last thing: If we have tentacles, shuffle the head vertex to sort vertically. */
  if (SPR->tentaclec>0) {
    int headp=0,i;
    for (i=0;i<SPR->tentaclec;i++) {
      if (spr->y>SPR->tentaclev[i].y) headp=i;
    }
    if (headp) {
      struct akgl_vtx_maxtile tmp=vtxv[0];
      memmove(vtxv,vtxv+1,sizeof(struct akgl_vtx_maxtile)*headp);
      vtxv[headp]=tmp;
    }
  }
  
  return vtxc;
}

/* Hurt.
 */

static int _ps_seamonster_hurt(struct ps_game *game,struct ps_sprite *spr,struct ps_sprite *assailant) {
  return 0;
}

/* Type definition.
 */

const struct ps_sprtype ps_sprtype_seamonster={
  .name="seamonster",
  .objlen=sizeof(struct ps_sprite_seamonster),

  .radius=PS_TILESIZE>>1,
  .shape=PS_SPRITE_SHAPE_CIRCLE,
  .layer=90,

  .init=_ps_seamonster_init,
  .del=_ps_seamonster_del,
  .configure=_ps_seamonster_configure,
  .update=_ps_seamonster_update,
  .draw=_ps_seamonster_draw,
  
};
