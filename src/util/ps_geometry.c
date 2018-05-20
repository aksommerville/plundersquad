#include "ps.h"
#include "ps_geometry.h"
#include <math.h>

/* Vector from direction and magnitude.
 */
 
struct ps_vector ps_vector_from_polar(int direction,int magnitude) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return ps_vector(0,-magnitude);
    case PS_DIRECTION_SOUTH: return ps_vector(0,magnitude);
    case PS_DIRECTION_WEST: return ps_vector(-magnitude,0);
    case PS_DIRECTION_EAST: return ps_vector(magnitude,0);
  }
  return ps_vector(0,0);
}

/* Get magnitude of vector.
 */
 
int ps_vector_get_magnitude(struct ps_vector vector) {
  if (vector.dx<0) vector.dx=-vector.dx;
  if (vector.dy<0) vector.dy=-vector.dy;
  if (vector.dx>=vector.dy) return vector.dx;
  return vector.dy;
}

/* Convert between direction and vector.
 */
 
int ps_direction_from_vector(struct ps_vector vector) {
  int adx=(vector.dx<0)?-vector.dx:vector.dx;
  int ady=(vector.dy<0)?-vector.dy:vector.dy;
  if (adx>ady) {
    if (vector.dx<0) return PS_DIRECTION_WEST;
    return PS_DIRECTION_EAST;
  } else {
    if (vector.dy<0) return PS_DIRECTION_NORTH;
    return PS_DIRECTION_SOUTH;
  }
}

int ps_direction_from_normal_vector(struct ps_vector vector) {
  if (vector.dx<0) return PS_DIRECTION_WEST;
  if (vector.dx>0) return PS_DIRECTION_EAST;
  if (vector.dy<0) return PS_DIRECTION_NORTH;
  return PS_DIRECTION_SOUTH;
}

struct ps_vector ps_vector_from_direction(int direction) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return ps_vector(0,-1);
    case PS_DIRECTION_SOUTH: return ps_vector(0,1);
    case PS_DIRECTION_WEST: return ps_vector(-1,0);
    case PS_DIRECTION_EAST: return ps_vector(1,0);
  }
  return ps_vector(0,0);
}

/* Direction operations.
 */
 
int ps_direction_reverse(int direction) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return PS_DIRECTION_SOUTH;
    case PS_DIRECTION_SOUTH: return PS_DIRECTION_NORTH;
    case PS_DIRECTION_WEST: return PS_DIRECTION_EAST;
    case PS_DIRECTION_EAST: return PS_DIRECTION_WEST;
  }
  return direction;
}

int ps_direction_rotate_clockwise(int direction) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return PS_DIRECTION_EAST;
    case PS_DIRECTION_SOUTH: return PS_DIRECTION_WEST;
    case PS_DIRECTION_WEST: return PS_DIRECTION_NORTH;
    case PS_DIRECTION_EAST: return PS_DIRECTION_SOUTH;
  }
  return direction;
}

int ps_direction_rotate_counterclockwise(int direction) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return PS_DIRECTION_WEST;
    case PS_DIRECTION_SOUTH: return PS_DIRECTION_EAST;
    case PS_DIRECTION_WEST: return PS_DIRECTION_SOUTH;
    case PS_DIRECTION_EAST: return PS_DIRECTION_NORTH;
  }
  return direction;
}

const char *ps_direction_repr(int direction) {
  switch (direction) {
    case PS_DIRECTION_NORTH: return "NORTH";
    case PS_DIRECTION_SOUTH: return "SOUTH";
    case PS_DIRECTION_WEST: return "WEST";
    case PS_DIRECTION_EAST: return "EAST";
  }
  return "unknown";
}

/* Convert between fvector and polcoord.
 */

struct ps_fvector ps_fvector_from_polcoord(struct ps_polcoord polcoord) {
  struct ps_fvector fvector;
  fvector.dx=cos(polcoord.angle)*polcoord.magnitude;
  fvector.dy=-sin(polcoord.angle)*polcoord.magnitude;
  return fvector;
}

struct ps_polcoord ps_polcoord_from_fvector(struct ps_fvector fvector) {
  struct ps_polcoord polcoord;
  polcoord.angle=ps_fvector_get_angle(fvector);
  polcoord.magnitude=ps_fvector_get_magnitude(fvector);
  return polcoord;
}

double ps_fvector_get_angle(struct ps_fvector fvector) {
  return atan2(fvector.dy,fvector.dx);
}

double ps_fvector_get_magnitude(struct ps_fvector fvector) {
  return sqrt(fvector.dx*fvector.dx+fvector.dy*fvector.dy);
}

/* Simple fvector arithmetic.
 */

struct ps_fvector ps_fvector_normalize(struct ps_fvector fvector) {
  return ps_fvector_div(fvector,ps_fvector_get_magnitude(fvector));
}

struct ps_fvector ps_fvector_add(struct ps_fvector a,struct ps_fvector b) {
  return ps_fvector(a.dx+b.dx,a.dy+b.dy);
}

struct ps_fvector ps_fvector_mlt(struct ps_fvector fvector,double n) {
  return ps_fvector(fvector.dx*n,fvector.dy*n);
}

struct ps_fvector ps_fvector_div(struct ps_fvector fvector,double n) {
  return ps_fvector(fvector.dx/n,fvector.dy/n);
}

/* Scalar projection.
 */

double ps_fvector_project(struct ps_fvector a,struct ps_fvector b) {
  return ps_fvector_project_unit(a,ps_fvector_normalize(b));
}

double ps_fvector_project_unit(struct ps_fvector a,struct ps_fvector b) {
  return a.dx*b.dx+a.dy*b.dy;
}

double ps_fvector_reject(struct ps_fvector a,struct ps_fvector b) {
  return ps_fvector_project_unit(a,ps_fvector_normalize(b));
}

double ps_fvector_reject_unit(struct ps_fvector a,struct ps_fvector b) {
  return a.dx*b.dy-a.dy*b.dx;
}

/* Convert between fbox and circle.
 */

struct ps_fbox ps_fbox_from_circle(struct ps_circle circle) {
  return ps_fbox(circle.x-circle.radius,circle.x+circle.radius,circle.y-circle.radius,circle.y+circle.radius);
}

struct ps_circle ps_circle_from_fbox(struct ps_fbox fbox) {
  double midx=(fbox.w+fbox.e)/2.0;
  double midy=(fbox.n+fbox.s)/2.0;
  double rx=midx-fbox.w;
  double ry=midy-fbox.n;
  return ps_circle(midx,midy,(rx>ry)?rx:ry);
}

/* Collide two rectangles.
 */

int ps_fbox_collide_fbox(struct ps_overlap *overlap,struct ps_fbox a,struct ps_fbox b) {

  if (a.e<=b.w) return 0;
  if (a.w>=b.e) return 0;
  if (a.s<=b.n) return 0;
  if (a.n>=b.s) return 0;
  
  if (overlap) {
    double penw=b.e-a.w;
    double pene=a.e-b.w;
    double penn=b.s-a.n;
    double pens=a.s-b.n;
    if ((penw<=pene)&&(penw<=penn)&&(penw<=pens)) {
      overlap->axis.dx=-1.0;
      overlap->axis.dy=0.0;
      overlap->penetration=penw;
    } else if ((pene<=penn)&&(pene<=pens)) {
      overlap->axis.dx=1.0;
      overlap->axis.dy=0.0;
      overlap->penetration=pene;
    } else if (penn<=pens) {
      overlap->axis.dx=0.0;
      overlap->axis.dy=-1.0;
      overlap->penetration=penn;
    } else {
      overlap->axis.dx=0.0;
      overlap->axis.dy=1.0;
      overlap->penetration=pens;
    }
  }

  return 1;
}

/* Collide rectangle against circle.
 */
 
int ps_fbox_collide_circle(struct ps_overlap *overlap,struct ps_fbox a,struct ps_circle b) {

  /* Rule it out if the bounding boxes don't collide. */
  struct ps_fbox bbox=ps_fbox_from_circle(b);
  if (a.n>=bbox.s) return 0;
  if (a.s<=bbox.n) return 0;
  if (a.w>=bbox.e) return 0;
  if (a.e<=bbox.w) return 0;

  /* Consider the circle's midpoint relative to the square.
   * It lies in one of nine regions, of three characters: internal, edge, or corner.
   * Internal is a collision, we can treat like squares.
   * Edge is a collision, and the circle is effectively a square.
   * Corner, we must test whether the square's corner lies inside the circle.
   */
  int relx,rely; // (-1,0,1) circle relative to square
  if (b.x<a.w) relx=-1;
  else if (b.x>a.e) relx=1;
  else relx=0;
  if (b.y<a.n) rely=-1;
  else if (b.y>a.s) rely=1;
  else rely=0;

  /* Circle's midpoint inside square. Proceed as if they are both squares. */
  if (!relx&&!rely) {
    if (overlap) {
      double penn=bbox.s-a.n;
      double pens=a.s-bbox.n;
      double penw=bbox.e-a.w;
      double pene=a.e-bbox.w;
      if ((penn<=pens)&&(penn<=penw)&&(penn<=pene)) {
        overlap->penetration=penn;
        overlap->axis.dx=0.0;
        overlap->axis.dy=-1.0;
      } else if ((pens<=pene)&&(pens<=penw)) {
        overlap->penetration=pens;
        overlap->axis.dx=0.0;
        overlap->axis.dy=1.0;
      } else if (pene<=penw) {
        overlap->penetration=pene;
        overlap->axis.dx=1.0;
        overlap->axis.dy=0.0;
      } else {
        overlap->penetration=penw;
        overlap->axis.dx=-1.0;
        overlap->axis.dy=0.0;
      }
    }
    return 1;
  }

  /* Edge collision. */
  if (!relx||!rely) {
    if (overlap) {
      if (relx==-1) {
        overlap->penetration=bbox.e-a.w;
        overlap->axis.dx=-1.0;
        overlap->axis.dy=0.0;
      } else if (relx==1) {
        overlap->penetration=a.e-bbox.w;
        overlap->axis.dx=1.0;
        overlap->axis.dy=0.0;
      } else if (rely==-1) {
        overlap->penetration=bbox.s-a.n;
        overlap->axis.dx=0.0;
        overlap->axis.dy=-1.0;
      } else {
        overlap->penetration=a.s-bbox.n;
        overlap->axis.dx=0.0;
        overlap->axis.dy=1.0;
      }
    }
    return 1;
  }

  /* Corners. */
  double cornerx,cornery;
  if (relx==-1) cornerx=a.w; else cornerx=a.e;
  if (rely==-1) cornery=a.n; else cornery=a.s;
  double dx=cornerx-b.x;
  double dy=cornery-b.y;
  double distance2=dx*dx+dy*dy;
  double radius2=b.radius*b.radius;
  if (distance2>=radius2) return 0;

  /* We have a corner collision. Calculate true distance and record it. */
  double distance=sqrt(distance2);
  double penetration=b.radius-distance;
  if (penetration<=0.0) return 0;

  if (overlap) {
    overlap->penetration=penetration;
    if (!distance) distance=1.0;
    overlap->axis.dx=(b.x-cornerx)/distance;
    overlap->axis.dy=(b.y-cornery)/distance;
    if (!overlap->axis.dx&&!overlap->axis.dy) overlap->axis.dy=1.0;
  }
  
  return 1;
}

int ps_circle_collide_fbox(struct ps_overlap *overlap,struct ps_circle a,struct ps_fbox b) {
  if (ps_fbox_collide_circle(overlap,b,a)<=0) return 0;
  if (overlap) {
    overlap->axis.dx=-overlap->axis.dx;
    overlap->axis.dy=-overlap->axis.dy;
  }
  return 1;
}

/* Collide circle against circle.
 */
 
int ps_circle_collide_circle(struct ps_overlap *overlap,struct ps_circle a,struct ps_circle b) {

  /* If more distant than the combined radii on either axis, end early. */
  double radiussum=a.radius+b.radius;
  double dx=a.x-b.x; if (dx<0.0) dx=-dx;
  if (dx>=radiussum) return 0;
  double dy=a.y-b.y; if (dy<0.0) dy=-dy;
  if (dy>=radiussum) return 0;

  /* Calculate true distance and compare again. */
  double distance2=dx*dx+dy*dy;
  if (distance2>=radiussum*radiussum) return 0;
  double distance=sqrt(distance2);
  double penetration=radiussum-distance;
  if (penetration<=0.0) return 0;

  /* Calculate unit vector if requested.
   * The zero-checks here are hugely important: If two circles are directly upon each other, we must not fail.
   */
  if (overlap) {
    overlap->penetration=penetration;
    if (!distance) distance=1.0;
    overlap->axis.dx=(b.x-a.x)/distance;
    overlap->axis.dy=(b.y-a.y)/distance;
    if (!overlap->axis.dx&&!overlap->axis.dy) overlap->axis.dy=1.0;
  }

  return 1;
}
