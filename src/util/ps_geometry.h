/* ps_geometry.h
 * Primitive helpers for dealing with 2-dimensional geometry.
 */

#ifndef PS_GEOMETRY_H
#define PS_GEOMETRY_H

/* Discrete geometry, used heavily by the scenario generator.
 *****************************************************************************/

#define PS_DIRECTION_NORTH   1
#define PS_DIRECTION_SOUTH   2
#define PS_DIRECTION_WEST    3
#define PS_DIRECTION_EAST    4

struct ps_vector {
  int dx;
  int dy;
};

static inline struct ps_vector ps_vector(int dx,int dy) {
  return (struct ps_vector){dx,dy};
}

struct ps_vector ps_vector_from_polar(int direction,int magnitude);

int ps_vector_get_magnitude(struct ps_vector vector);

/* Convert between scalar "direction" and vectors.
 * "normal_vector" is presumed to have only one nonzero axis.
 */
int ps_direction_from_vector(struct ps_vector vector);
int ps_direction_from_normal_vector(struct ps_vector vector);
struct ps_vector ps_vector_from_direction(int direction);

int ps_direction_reverse(int direction);
int ps_direction_rotate_clockwise(int direction);
int ps_direction_rotate_counterclockwise(int direction);

static inline int ps_direction_is_vertical(int direction) { return ((direction==PS_DIRECTION_NORTH)||(direction==PS_DIRECTION_SOUTH)); }
static inline int ps_direction_is_horizontal(int direction) { return ((direction==PS_DIRECTION_WEST)||(direction==PS_DIRECTION_EAST)); }

const char *ps_direction_repr(int direction);

/* Floating-point geometry, used heavily by the physics engine.
 *****************************************************************************/

struct ps_fvector {
  double dx;
  double dy;
};

struct ps_polcoord {
  double angle; // radians
  double magnitude;
};

struct ps_fbox {
  double w,e,n,s;
};

struct ps_circle {
  double x,y,radius;
};

struct ps_overlap {
  struct ps_fvector axis; // Unit vector pointing from (a) to (b).
  double penetration; // Amount of overlap. (a) moves by -(penetration) or (b) by (penetration), along (axis).
};

static inline struct ps_fvector ps_fvector(double dx,double dy) {
  return (struct ps_fvector){dx,dy};
}

static inline struct ps_polcoord ps_polcoord(double angle,double magnitude) {
  return (struct ps_polcoord){angle,magnitude};
}

static inline struct ps_fbox ps_fbox(double w,double e,double n,double s) {
  return (struct ps_fbox){w,e,n,s};
}

static inline struct ps_circle ps_circle(double x,double y,double radius) {
  return (struct ps_circle){x,y,radius};
}

struct ps_fvector ps_fvector_from_polcoord(struct ps_polcoord polcoord);
struct ps_polcoord ps_polcoord_from_fvector(struct ps_fvector fvector);

double ps_fvector_get_angle(struct ps_fvector fvector);
double ps_fvector_get_magnitude(struct ps_fvector fvector);
struct ps_fvector ps_fvector_normalize(struct ps_fvector fvector);
struct ps_fvector ps_fvector_add(struct ps_fvector a,struct ps_fvector b);
struct ps_fvector ps_fvector_mlt(struct ps_fvector fvector,double n);
struct ps_fvector ps_fvector_div(struct ps_fvector fvector,double n);

/* Scalar projection and rejection.
 * This is easier to calculate from a unit vector.
 * So we provide a second "_unit" version, where (b) is a unit vector.
 */
double ps_fvector_project(struct ps_fvector a,struct ps_fvector b);
double ps_fvector_project_unit(struct ps_fvector a,struct ps_fvector b);
double ps_fvector_reject(struct ps_fvector a,struct ps_fvector b);
double ps_fvector_reject_unit(struct ps_fvector a,struct ps_fvector b);

struct ps_fbox ps_fbox_from_circle(struct ps_circle circle);
struct ps_circle ps_circle_from_fbox(struct ps_fbox fbox);

/* 0 if no collision or 1 if collision.
 * (overlap) is optional; don't ask for it if you don't want it.
 */
int ps_fbox_collide_fbox(struct ps_overlap *overlap,struct ps_fbox a,struct ps_fbox b);
int ps_fbox_collide_circle(struct ps_overlap *overlap,struct ps_fbox a,struct ps_circle b);
int ps_circle_collide_fbox(struct ps_overlap *overlap,struct ps_circle a,struct ps_fbox b);
int ps_circle_collide_circle(struct ps_overlap *overlap,struct ps_circle a,struct ps_circle b);

#endif
