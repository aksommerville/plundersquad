#include "ps.h"
#include "ps_physics.h"
#include "ps_sprite.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include <math.h>

#define PS_PHYSICS_EPSILON       0.001 /* Penetrations smaller than this are discarded. */
#define PS_PHYSICS_ACTION_BIAS   1.010 /* Any time we move a sprite, fudge the movement outward just a little. */
#define PS_PHYSICS_REPC 10 /* Repeat each update so many times or until the world is stable. */
#define PS_PHYSICS_GRID_OOBW 1 /* Copy grid's nearest neighbor to so many cells offscreen, then treat them all as solid. */

/* Object lifecycle.
 */

struct ps_physics *ps_physics_new() {
  struct ps_physics *physics=calloc(1,sizeof(struct ps_physics));
  if (!physics) return 0;
  return physics;
}

void ps_physics_del(struct ps_physics *physics) {
  if (!physics) return;
  ps_sprgrp_del(physics->grp_physics);
  ps_sprgrp_del(physics->grp_solid);
  ps_grid_del(physics->grid);
  if (physics->collv) free(physics->collv);
  if (physics->eventv) free(physics->eventv);
  free(physics);
}

/* Member accessors.
 */
 
int ps_physics_set_sprgrp_physics(struct ps_physics *physics,struct ps_sprgrp *grp) {
  if (!physics) return -1;
  if (grp&&(ps_sprgrp_ref(grp)<0)) return -1;
  ps_sprgrp_del(physics->grp_physics);
  physics->grp_physics=grp;
  return 0;
}
 
int ps_physics_set_sprgrp_solid(struct ps_physics *physics,struct ps_sprgrp *grp) {
  if (!physics) return -1;
  if (grp&&(ps_sprgrp_ref(grp)<0)) return -1;
  ps_sprgrp_del(physics->grp_solid);
  physics->grp_solid=grp;
  return 0;
}

int ps_physics_set_grid(struct ps_physics *physics,struct ps_grid *grid) {
  if (!physics) return -1;
  if (grid&&(ps_grid_ref(grid)<0)) return -1;
  ps_grid_del(physics->grid);
  physics->grid=grid;
  return 0;
}

/* Search events.
 */

static int ps_physics_search_event(const struct ps_physics *physics,const struct ps_sprite *a,const struct ps_sprite *b) {
  int lo=0,hi=physics->eventc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (a<physics->eventv[ck].a) hi=ck;
    else if (a>physics->eventv[ck].a) lo=ck+1;
    else if (b<physics->eventv[ck].b) hi=ck;
    else if (b>physics->eventv[ck].b) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add event record.
 */

static int ps_physics_add_event(struct ps_physics *physics,struct ps_sprite *a,struct ps_sprite *b) {
  if (!physics||(!a&&!b)) return -1;

  if (a>b) {
    struct ps_sprite *tmp=a;
    a=b;
    b=tmp;
  }
  
  int p=ps_physics_search_event(physics,a,b);
  if (p>=0) return 0;
  p=-p-1;
  
  if (physics->eventc>=physics->eventa) {
    int na=physics->eventa+32;
    if (na>INT_MAX/sizeof(struct ps_physics_event)) return -1;
    void *nv=realloc(physics->eventv,sizeof(struct ps_physics_event)*na);
    if (!nv) return -1;
    physics->eventv=nv;
    physics->eventa=na;
  }

  struct ps_physics_event *event=physics->eventv+p;
  memmove(event+1,event,sizeof(struct ps_physics_event)*(physics->eventc-p));
  physics->eventc++;
  event->a=a;
  event->b=b;

  return 0;
}

/* Add collision record.
 */

static struct ps_coll *ps_physics_add_coll(struct ps_physics *physics) {
  if (physics->collc>=physics->colla) {
    int na=physics->colla+64;
    if (na>INT_MAX/sizeof(struct ps_coll)) return 0;
    void *nv=realloc(physics->collv,sizeof(struct ps_coll)*na);
    if (!nv) return 0;
    physics->collv=nv;
    physics->colla=na;
  }
  struct ps_coll *coll=physics->collv+physics->collc;
  physics->collc++;
  memset(coll,0,sizeof(struct ps_coll));
  return coll;
}

/* Unset sprite flags, before all cycles.
 */

static int ps_physics_unset_sprite_flags(struct ps_physics *physics) {
  if (physics->grp_physics) {
    int i=physics->grp_physics->sprc; while (i-->0) {
      struct ps_sprite *spr=physics->grp_physics->sprv[i];
      spr->collided_grid=0;
    }
  }
  return 0;
}

/* Begin an update cycle.
 */

static int ps_physics_begin(struct ps_physics *physics) {
  int i;
  if (!physics) return -1;
  
  physics->collc=0;

  if (physics->grp_physics) {
    for (i=physics->grp_physics->sprc;i-->0;) physics->grp_physics->sprv[i]->phreconsider=0;
  }
  if (physics->grp_solid) {
    for (i=physics->grp_solid->sprc;i-->0;) physics->grp_solid->sprv[i]->phreconsider=0;
  }
  
  return 0;
}

/* Compose geometric shape from sprite.
 */

static inline struct ps_fbox ps_sprite_fbox(const struct ps_sprite *spr) {
  return ps_fbox(spr->x-spr->radius,spr->x+spr->radius,spr->y-spr->radius,spr->y+spr->radius);
}

static inline struct ps_circle ps_sprite_circle(const struct ps_sprite *spr) {
  return ps_circle(spr->x,spr->y,spr->radius);
}

/* Detect collision between two sprites: dispatch on shapes.
 */
 
static int ps_physics_check_sprites(struct ps_physics *physics,struct ps_sprite *a,struct ps_sprite *b) {
  if ((a->type==b->type)&&a->type->ignore_collisions_on_same_type) return 0;
  struct ps_overlap overlap;
  switch (a->shape) {
    case PS_SPRITE_SHAPE_SQUARE: switch (b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_fbox_collide_fbox(&overlap,ps_sprite_fbox(a),ps_sprite_fbox(b))) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_fbox_collide_circle(&overlap,ps_sprite_fbox(a),ps_sprite_circle(b))) return 0; break;
        default: return -1;
      } break;
    case PS_SPRITE_SHAPE_CIRCLE: switch (b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_circle_collide_fbox(&overlap,ps_sprite_circle(a),ps_sprite_fbox(b))) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_circle_collide_circle(&overlap,ps_sprite_circle(a),ps_sprite_circle(b))) return 0; break;
        default: return -1;
      } break;
    default: return -1;
  }
  if (overlap.penetration<=PS_PHYSICS_EPSILON) return 0;
  struct ps_coll *coll=ps_physics_add_coll(physics);
  if (!coll) return -1;
  coll->a=a;
  coll->b=b;
  coll->overlap=overlap;
  if (ps_physics_add_event(physics,a,b)<0) return -1;
  return 1;
}

static int ps_physics_recheck_sprites(struct ps_physics *physics,struct ps_coll *coll) {
  switch (coll->a->shape) {
    case PS_SPRITE_SHAPE_SQUARE: switch (coll->b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_fbox_collide_fbox(&coll->overlap,ps_sprite_fbox(coll->a),ps_sprite_fbox(coll->b))) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_fbox_collide_circle(&coll->overlap,ps_sprite_fbox(coll->a),ps_sprite_circle(coll->b))) return 0; break;
        default: return 0;
      } break;
    case PS_SPRITE_SHAPE_CIRCLE: switch (coll->b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_circle_collide_fbox(&coll->overlap,ps_sprite_circle(coll->a),ps_sprite_fbox(coll->b))) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_circle_collide_circle(&coll->overlap,ps_sprite_circle(coll->a),ps_sprite_circle(coll->b))) return 0; break;
        default: return 0;
      } break;
    default: return 0;
  }
  if (coll->overlap.penetration<=PS_PHYSICS_EPSILON) return 0;
  return 1;
}

/* Which grid cells do we care about?
 */

static inline int ps_physics_cell_is_solid(uint8_t cell_physics,uint16_t impassable) {
  return (impassable&(1<<cell_physics));
}

/* Get effective shape of a grid cell based on sprite's position.
 * (dx,dy) is the sprite's position relative to the cell's center.
 */

static int ps_physics_get_effective_cell_shape(uint8_t shape,double dx,double dy) {
  if (!shape) return PS_SPRITE_SHAPE_SQUARE; // Most common case.
  if (dx<0.0) {
    if (dy<0.0) {
      return (shape&PS_GRID_CELL_SHAPE_RNW)?PS_SPRITE_SHAPE_CIRCLE:PS_SPRITE_SHAPE_SQUARE;
    } else if (dy>0.0) {
      return (shape&PS_GRID_CELL_SHAPE_RSW)?PS_SPRITE_SHAPE_CIRCLE:PS_SPRITE_SHAPE_SQUARE;
    }
  } else if (dx>0.0) {
    if (dy<0.0) {
      return (shape&PS_GRID_CELL_SHAPE_RNE)?PS_SPRITE_SHAPE_CIRCLE:PS_SPRITE_SHAPE_SQUARE;
    } else if (dy>0.0) {
      return (shape&PS_GRID_CELL_SHAPE_RSE)?PS_SPRITE_SHAPE_CIRCLE:PS_SPRITE_SHAPE_SQUARE;
    }
  }
  return PS_SPRITE_SHAPE_SQUARE;
}

/* Look at any collisions we've already recorded, return nonzero if this sprite touches a hero.
 */

static int ps_physics_sprite_currently_collides_with_hero(const struct ps_physics *physics,const struct ps_sprite *spr) {
  const struct ps_coll *coll=physics->collv;
  int i=physics->collc; for (;i-->0;coll++) {
    if (!coll->a) continue;
    if (!coll->b) continue;
    if (coll->a==spr) {
      if (coll->b->type==&ps_sprtype_hero) return 1;
    } else if (coll->b==spr) {
      if (coll->a->type==&ps_sprtype_hero) return 1;
    }
  }
  return 0;
}

/* Check sprite collisions against the grid.
 * For now, we treat each cell like an independent sprite.
 * If we add velocity, this might be more complicated.
 */

static int ps_physics_check_grid(struct ps_physics *physics,struct ps_sprite *spr) {
  if (!spr->impassable) return 0;

  /* Find the sprite's bounding box and convert to cells.
   * It's OK if these exceed the grid boundaries.
   */
  int xa=spr->x-spr->radius;
  int xz=spr->x+spr->radius-1;
  int ya=spr->y-spr->radius;
  int yz=spr->y+spr->radius-1;
  int cola=xa/PS_TILESIZE; if (xa<0) cola--;
  int colz=xz/PS_TILESIZE; if (xz<0) colz--;
  int rowa=ya/PS_TILESIZE; if (ya<0) rowa--;
  int rowz=yz/PS_TILESIZE; if (yz<0) rowz--;

  /* Iterate over the covered cells.
   * We need the true (col,row), ie possibly OOB, to compare pixelwise against the sprite.
   * But the cell data we'll use is more complicated:
   *  - Anything in bounds, we use it as expected.
   *  - The immediate outer rings (PS_PHYSICS_OOBW) uses its nearest neighbor.
   *  - Anything more than PS_PHYSICS_OOBW cells OOB is effectively (0,0), which we presume to be solid.
   * The desired effect is that sprites can walk offscreen, but no more than one cell away.
   */
  int row=rowa; for (;row<=rowz;row++) {
    int col=cola; for (;col<=colz;col++) {

      int effective_row,effective_col;
      if ((row<-PS_PHYSICS_GRID_OOBW)||(col<-PS_PHYSICS_GRID_OOBW)||(row>=PS_GRID_ROWC+PS_PHYSICS_GRID_OOBW)||(col>=PS_GRID_COLC+PS_PHYSICS_GRID_OOBW)) {
        effective_row=0;
        effective_col=0;
      } else {
        if (row<0) effective_row=0;
        else if (row>=PS_GRID_ROWC) effective_row=PS_GRID_ROWC-1;
        else effective_row=row;
        if (col<0) effective_col=0;
        else if (col>=PS_GRID_COLC) effective_col=PS_GRID_COLC-1;
        else effective_col=col;
      }
      const struct ps_grid_cell *effective_cell=physics->grid->cellv+effective_row*PS_GRID_COLC+effective_col;

      if (!ps_physics_cell_is_solid(effective_cell->physics,spr->impassable)) continue;

      struct ps_fbox cellbox=ps_fbox(col*PS_TILESIZE,(col+1)*PS_TILESIZE,row*PS_TILESIZE,(row+1)*PS_TILESIZE);
      struct ps_circle cellcircle=ps_circle(col*PS_TILESIZE+(PS_TILESIZE>>1),row*PS_TILESIZE+(PS_TILESIZE>>1),PS_TILESIZE>>1);
      int cellshape=ps_physics_get_effective_cell_shape(effective_cell->shape,spr->x-cellcircle.x,spr->y-cellcircle.y);
      
      struct ps_overlap overlap;
      switch (spr->shape) {
        case PS_SPRITE_SHAPE_SQUARE: switch (cellshape) {
            case PS_SPRITE_SHAPE_SQUARE: if (!ps_fbox_collide_fbox(&overlap,ps_sprite_fbox(spr),cellbox)) continue; break;
            case PS_SPRITE_SHAPE_CIRCLE: if (!ps_fbox_collide_circle(&overlap,ps_sprite_fbox(spr),cellcircle)) continue; break;
          } break;
        case PS_SPRITE_SHAPE_CIRCLE: switch (cellshape) {
            case PS_SPRITE_SHAPE_SQUARE: if (!ps_circle_collide_fbox(&overlap,ps_sprite_circle(spr),cellbox)) continue; break;
            case PS_SPRITE_SHAPE_CIRCLE: if (!ps_circle_collide_circle(&overlap,ps_sprite_circle(spr),cellcircle)) continue; break;
          } break;
        default: return -1;
      }
      if (overlap.penetration<=PS_PHYSICS_EPSILON) continue;

      /* Record it. */
      spr->collided_grid=1;
      struct ps_coll *coll=ps_physics_add_coll(physics);
      if (!coll) return -1;
      coll->a=spr;
      coll->b=0;
      coll->col=col;
      coll->row=row;
      coll->cellshape=cellshape;
      coll->overlap=overlap;
      if (ps_physics_add_event(physics,spr,0)<0) return -1;
    }
  }

  return 0;
}

/* Recheck single grid cell.
 */

static int ps_physics_recheck_grid(struct ps_physics *physics,struct ps_coll *coll) {
return 1;
  struct ps_fbox cellbox=ps_fbox(coll->col*PS_TILESIZE,(coll->col+1)*PS_TILESIZE,coll->row*PS_TILESIZE,(coll->row+1)*PS_TILESIZE);
  struct ps_circle cellcircle=ps_circle(coll->col*PS_TILESIZE+(PS_TILESIZE>>1),coll->row*PS_TILESIZE+(PS_TILESIZE>>1),PS_TILESIZE>>1);
  switch (coll->a->shape) {
    case PS_SPRITE_SHAPE_SQUARE: switch (coll->cellshape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_fbox_collide_fbox(&coll->overlap,ps_sprite_fbox(coll->a),cellbox)) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_fbox_collide_circle(&coll->overlap,ps_sprite_fbox(coll->a),cellcircle)) return 0; break;
      } break;
    case PS_SPRITE_SHAPE_CIRCLE: switch (coll->cellshape) {
        case PS_SPRITE_SHAPE_SQUARE: if (!ps_circle_collide_fbox(&coll->overlap,ps_sprite_circle(coll->a),cellbox)) return 0; break;
        case PS_SPRITE_SHAPE_CIRCLE: if (!ps_circle_collide_circle(&coll->overlap,ps_sprite_circle(coll->a),cellcircle)) return 0; break;
      } break;
    default: return 0;
  }
  if (coll->overlap.penetration<=PS_PHYSICS_EPSILON) return 0;
  return 1;
}

/* Check collisions for one sprite.
 * (sprindex) is the first index to check; only examine sprites at a higher index than myself.
 */

static int ps_physics_detect_collisions_1(struct ps_physics *physics,struct ps_sprite *spr,int sprindex) {
  int i;

  /* Check other sprites. */
  if (spr->collide_sprites) {
    for (i=0;i<physics->grp_solid->sprc;i++) {
      struct ps_sprite *other=physics->grp_solid->sprv[i];
      if (spr==other) continue;
      if (!other->collide_sprites) continue;
      if (ps_physics_check_sprites(physics,spr,other)<0) return -1;
    }
  }

  /* Check grid. */
  if (physics->grid) {
    if (ps_physics_check_grid(physics,spr)<0) return -1;
  }

  return 0;
}

/* Detect collisions.
 */

static int ps_physics_detect_collisions(struct ps_physics *physics) {

  if (physics->grid) {
    int i=0; for (;i<physics->grp_physics->sprc;i++) {
      struct ps_sprite *spr=physics->grp_physics->sprv[i];
      if (ps_physics_check_grid(physics,spr)<0) return -1;
    }
  }

  if (physics->grp_solid) {
    int i=0; for (;i<physics->grp_solid->sprc;i++) {
      struct ps_sprite *a=physics->grp_solid->sprv[i];
      if (!a->collide_sprites) continue;
      int j=i+1; for (;j<physics->grp_solid->sprc;j++) {
        struct ps_sprite *b=physics->grp_solid->sprv[j];
        if (!b->collide_sprites) continue;
        if (ps_physics_check_sprites(physics,a,b)<0) return -1;
      }
    }
  }

  return 0;
}

/* Resolve one collision.
 */

static int ps_physics_resolve_collision(struct ps_physics *physics,struct ps_coll *coll) {

  //ps_log(PHYSICS,TRACE,"Bump! pen=%f v=(%+f,%+f)",coll->pen,coll->vx,coll->vy);

  /* If either (phreconsider) flag is set, reassess the collision. */
  if (coll->a->phreconsider||(coll->b&&coll->b->phreconsider)) {
    if (coll->b) {
      if (!ps_physics_recheck_sprites(physics,coll)) return 0;
    } else {
      if (!ps_physics_recheck_grid(physics,coll)) return 0;
    }
  }
  coll->a->phreconsider=1;
  if (coll->b) coll->b->phreconsider=1;

  /* Decide how much each body will move.
   */
  double ashare,bshare;
  if (!coll->b) { // Grid collision; A takes the whole impact.
    ashare=1.0;
    bshare=0.0;
  } else { // Force splits evenly. We could add per-sprite mass here if we felt like it.
    ashare=0.5;
    bshare=0.5;
  }

  /* Apply correction to positions. */
  coll->a->x+=ashare*coll->overlap.penetration*-coll->overlap.axis.dx*PS_PHYSICS_ACTION_BIAS;
  coll->a->y+=ashare*coll->overlap.penetration*-coll->overlap.axis.dy*PS_PHYSICS_ACTION_BIAS;
  if (coll->b) {
    coll->b->x+=bshare*coll->overlap.penetration*coll->overlap.axis.dx*PS_PHYSICS_ACTION_BIAS;
    coll->b->y+=bshare*coll->overlap.penetration*coll->overlap.axis.dy*PS_PHYSICS_ACTION_BIAS;
  }

  return 0;
}

/* Resolve collisions.
 */

static int ps_physics_resolve_collisions(struct ps_physics *physics) {
  struct ps_coll *coll=physics->collv;
  int i=physics->collc;
  for (;i-->0;coll++) {
    if (ps_physics_resolve_collision(physics,coll)<0) return -1;
  }
  return 0;
}

/* Update, main entry point.
 */
 
int ps_physics_update(struct ps_physics *physics) {
  if (!physics) return -1;

  physics->eventc=0;
  if (ps_physics_unset_sprite_flags(physics)<0) return -1;

  int repp=PS_PHYSICS_REPC;
  while (repp-->0) {

    /* Clear our transient state and terminate if there's nothing to do. */
    if (ps_physics_begin(physics)<0) return -1;
    if (!physics->grp_physics) return 0;
    if (physics->grp_physics->sprc<1) return 0;

    /* Detect collisions and terminate if there aren't any. */
    if (ps_physics_detect_collisions(physics)<0) return -1;
    if (!physics->collc) return 0;

    /* Resolve collisions. */
    if (ps_physics_resolve_collisions(physics)<0) return -1;

  }
  
  return 0;
}

/* Test a sprite collision (public interface).
 * This considers shape and position; it doesn't care about SOLID group or anything.
 */
 
int ps_sprites_collide(const struct ps_sprite *a,const struct ps_sprite *b) {
  if (!a||!b) return 0;
  if (a==b) return 1;
  switch (a->shape) {
    case PS_SPRITE_SHAPE_SQUARE: switch (b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: return ps_fbox_collide_fbox(0,ps_sprite_fbox(a),ps_sprite_fbox(b));
        case PS_SPRITE_SHAPE_CIRCLE: return ps_fbox_collide_circle(0,ps_sprite_fbox(a),ps_sprite_circle(b));
      } break;
    case PS_SPRITE_SHAPE_CIRCLE: switch (b->shape) {
        case PS_SPRITE_SHAPE_SQUARE: return ps_circle_collide_fbox(0,ps_sprite_circle(a),ps_sprite_fbox(b));
        case PS_SPRITE_SHAPE_CIRCLE: return ps_circle_collide_circle(0,ps_sprite_circle(a),ps_sprite_circle(b));
      } break;
  }
  return 0;
}

int ps_sprite_collide_fbox(const struct ps_sprite *spr,const struct ps_fbox *fbox) {
  if (!spr||!fbox) return 0;
  switch (spr->shape) {
    case PS_SPRITE_SHAPE_SQUARE: return ps_fbox_collide_fbox(0,ps_sprite_fbox(spr),*fbox);
    case PS_SPRITE_SHAPE_CIRCLE: return ps_circle_collide_fbox(0,ps_sprite_circle(spr),*fbox);
  }
  return 0;
}

int ps_sprite_collide_circle(const struct ps_sprite *spr,const struct ps_circle *circle) {
  if (!spr||!circle) return 0;
  switch (spr->shape) {
    case PS_SPRITE_SHAPE_SQUARE: return ps_fbox_collide_circle(0,ps_sprite_fbox(spr),*circle);
    case PS_SPRITE_SHAPE_CIRCLE: return ps_circle_collide_circle(0,ps_sprite_circle(spr),*circle);
  }
  return 0;
}
