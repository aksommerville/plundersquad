/* ps_physics.h
 * Interaction among sprites and grid.
 * Physics are rectified at the end of each update cycle.
 * Only sprites in the appropriate group (PS_SPRGRP_PHYSICS) participate.
 */

#ifndef PS_PHYSICS_H
#define PS_PHYSICS_H

#include "util/ps_geometry.h"

struct ps_sprite;
struct ps_sprgrp;
struct ps_sprtype;
struct ps_grid;

struct ps_coll {
  struct ps_sprite *a,*b; // WEAK; (b) is null if against the grid
  uint8_t col,row,cellshape; // Valid if (b) is null.
  struct ps_overlap overlap;
};

// We record every collision for the caller's perusal after the fact.
struct ps_physics_event {
  struct ps_sprite *a,*b; // WEAK; (a) is null if against the grid; sorted (a,b); (a<b) always
};

struct ps_physics {
  struct ps_sprgrp *grp_physics;
  struct ps_sprgrp *grp_solid;
  struct ps_grid *grid;
  struct ps_coll *collv;
  int collc,colla;
  struct ps_physics_event *eventv;
  int eventc,eventa;
};

struct ps_physics *ps_physics_new();
void ps_physics_del(struct ps_physics *physics);

int ps_physics_set_sprgrp_physics(struct ps_physics *physics,struct ps_sprgrp *grp);
int ps_physics_set_sprgrp_solid(struct ps_physics *physics,struct ps_sprgrp *grp);
int ps_physics_set_grid(struct ps_physics *physics,struct ps_grid *grid);

int ps_physics_update(struct ps_physics *physics);

/* Return nonzero if (spr) in the last update collided with something.
 */
int ps_physics_test_sprite_collision_grid(const struct ps_physics *physics,const struct ps_sprite *spr);
int ps_physics_test_sprite_collision_sprite(const struct ps_physics *physics,const struct ps_sprite *spr);
int ps_physics_test_sprite_collision_any(const struct ps_physics *physics,const struct ps_sprite *spr);
int ps_physics_test_sprite_collision_exact(const struct ps_physics *physics,const struct ps_sprite *a,const struct ps_sprite *b);
int ps_physics_test_sprite_collision_type(const struct ps_physics *physics,const struct ps_sprite *a,const struct ps_sprtype *btype);

#endif
