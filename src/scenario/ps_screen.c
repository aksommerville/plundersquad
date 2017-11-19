#include "ps.h"
#include "ps_screen.h"
#include "ps_blueprint.h"
#include "ps_blueprint_list.h"
#include "ps_grid.h"

/* Cleanup.
 */

void ps_screen_cleanup(struct ps_screen *screen) {
  if (!screen) return;

  ps_blueprint_del(screen->blueprint);
  ps_grid_del(screen->grid);

  memset(screen,0,sizeof(struct ps_screen));
}

/* Trivial accessors.
 */
 
int ps_screen_set_blueprint(struct ps_screen *screen,struct ps_blueprint *blueprint) {
  if (!screen) return -1;
  if (blueprint&&(ps_blueprint_ref(blueprint)<0)) return -1;
  ps_blueprint_del(screen->blueprint);
  screen->blueprint=blueprint;
  return 0;
}

/* Accessors with direction.
 */
 
int ps_screen_door_for_direction(const struct ps_screen *screen,int direction) {
  if (!screen) return PS_DOOR_UNSET;
  switch (direction) {
    case PS_DIRECTION_NORTH: return screen->doorn;
    case PS_DIRECTION_SOUTH: return screen->doors;
    case PS_DIRECTION_WEST: return screen->doorw;
    case PS_DIRECTION_EAST: return screen->doore;
  }
  return PS_DOOR_UNSET;
}

int ps_screen_set_door_for_direction(struct ps_screen *screen,int direction,int door) {
  if (!screen) return -1;
  switch (direction) {
    case PS_DIRECTION_NORTH: screen->doorn=door; return 0;
    case PS_DIRECTION_SOUTH: screen->doors=door; return 0;
    case PS_DIRECTION_WEST: screen->doorw=door; return 0;
    case PS_DIRECTION_EAST: screen->doore=door; return 0;
  }
  return -1;
}

struct ps_screen *ps_screen_neighbor_for_direction(struct ps_screen *screen,int direction,int worldw) {
  if (!screen) return 0;
  switch (direction) {
    case PS_DIRECTION_NORTH: {
        if (screen->doorn==PS_DOOR_CLOSED) return 0;
        return screen-worldw;
      }
    case PS_DIRECTION_SOUTH: {
        if (screen->doors==PS_DOOR_CLOSED) return 0;
        return screen+worldw;
      }
    case PS_DIRECTION_WEST: {
        if (screen->doorw==PS_DOOR_CLOSED) return 0;
        return screen-1;
      }
    case PS_DIRECTION_EAST: {
        if (screen->doore==PS_DOOR_CLOSED) return 0;
        return screen+1;
      }
  }
  return 0;
}

/* Count of open doors.
 */
 
int ps_screen_count_doors(const struct ps_screen *screen) {
  if (!screen) return 0;
  int c=0;
  if (screen->doorn==PS_DOOR_OPEN) c++;
  if (screen->doors==PS_DOOR_OPEN) c++;
  if (screen->doorw==PS_DOOR_OPEN) c++;
  if (screen->doore==PS_DOOR_OPEN) c++;
  return c;
}

/* Get single neighbor.
 */
 
struct ps_screen *ps_screen_get_single_neighbor(struct ps_screen *screen,int worldw) {
  if (!screen) return 0;
  if (screen->doorn==PS_DOOR_OPEN) {
    if (screen->doors==PS_DOOR_OPEN) return 0;
    if (screen->doorw==PS_DOOR_OPEN) return 0;
    if (screen->doore==PS_DOOR_OPEN) return 0;
    return screen-worldw;
  }
  if (screen->doors==PS_DOOR_OPEN) {
    if (screen->doorn==PS_DOOR_OPEN) return 0;
    if (screen->doorw==PS_DOOR_OPEN) return 0;
    if (screen->doore==PS_DOOR_OPEN) return 0;
    return screen+worldw;
  }
  if (screen->doorw==PS_DOOR_OPEN) {
    if (screen->doors==PS_DOOR_OPEN) return 0;
    if (screen->doorn==PS_DOOR_OPEN) return 0;
    if (screen->doore==PS_DOOR_OPEN) return 0;
    return screen-1;
  }
  if (screen->doore==PS_DOOR_OPEN) {
    if (screen->doors==PS_DOOR_OPEN) return 0;
    if (screen->doorw==PS_DOOR_OPEN) return 0;
    if (screen->doorn==PS_DOOR_OPEN) return 0;
    return screen+1;
  }
  return 0;
}

/* Get single awayward direction if present.
 */
 
int ps_screen_get_single_awayward_direction(const struct ps_screen *screen) {
  if (!screen) return -1;
  switch (screen->direction_home) {

    #define UP_UP_AND_AWAYWARD(v1,v2,v3,d1,d2,d3) { \
      if ((screen->v1==PS_DOOR_OPEN)&&(screen->v2==PS_DOOR_CLOSED)&&(screen->v3==PS_DOOR_CLOSED)) return PS_DIRECTION_##d1; \
      if ((screen->v1==PS_DOOR_CLOSED)&&(screen->v2==PS_DOOR_OPEN)&&(screen->v3==PS_DOOR_CLOSED)) return PS_DIRECTION_##d2; \
      if ((screen->v1==PS_DOOR_CLOSED)&&(screen->v2==PS_DOOR_CLOSED)&&(screen->v3==PS_DOOR_OPEN)) return PS_DIRECTION_##d3; \
    } break;
  
    case PS_DIRECTION_NORTH: UP_UP_AND_AWAYWARD(doore,doorw,doors,EAST,WEST,SOUTH)
    case PS_DIRECTION_SOUTH: UP_UP_AND_AWAYWARD(doore,doorw,doorn,EAST,WEST,NORTH)
    case PS_DIRECTION_WEST: UP_UP_AND_AWAYWARD(doore,doorn,doors,EAST,NORTH,SOUTH)
    case PS_DIRECTION_EAST: UP_UP_AND_AWAYWARD(doorn,doorw,doors,NORTH,WEST,SOUTH)

    #undef UP_UP_AND_AWAYWARD
  }
  return -1;
}

/* Build inner grid.
 */
 
int ps_screen_build_inner_grid(struct ps_screen *screen) {
  if (!screen) return -1;
  if (!screen->blueprint) return -1;
  if (screen->grid) ps_grid_del(screen->grid);
  if (!(screen->grid=ps_grid_new())) return -1;

  /* Copy from blueprint->cellv to grid->cellv.physics, respecting xform. */
  const uint8_t *srcrow=screen->blueprint->cellv;
  int srcrowd=PS_BLUEPRINT_COLC;
  if (screen->xform&PS_BLUEPRINT_XFORM_VERT) {
    srcrow=screen->blueprint->cellv+PS_BLUEPRINT_SIZE-PS_BLUEPRINT_COLC;
    srcrowd=-PS_BLUEPRINT_COLC;
  }
  struct ps_grid_cell *dstrow=screen->grid->cellv+(PS_GRID_COLC*2)+2;
  int y=PS_BLUEPRINT_ROWC; for (;y-->0;srcrow+=srcrowd,dstrow+=PS_GRID_COLC) {
    if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) {
      int dstx=PS_BLUEPRINT_COLC-1,srcx=0; for (;srcx<PS_BLUEPRINT_COLC;dstx--,srcx++) {
        dstrow[dstx].physics=srcrow[srcx];
      }
    } else {
      int x=0; for (;x<PS_BLUEPRINT_COLC;x++) {
        dstrow[x].physics=srcrow[x];
      }
    }
  }

  /* Copy POI from blueprint, verbatim at first. */
  if (screen->blueprint->poic>0) {
    if (!(screen->grid->poiv=malloc(sizeof(struct ps_blueprint_poi)*screen->blueprint->poic))) return -1;
    memcpy(screen->grid->poiv,screen->blueprint->poiv,sizeof(struct ps_blueprint_poi)*screen->blueprint->poic);
    screen->grid->poic=screen->blueprint->poic;

    /* Apply xform to POI positions, and offset them by 2 for the frame. */
    struct ps_blueprint_poi *poi=poi=screen->grid->poiv;
    int i=screen->grid->poic; for (;i-->0;poi++) {
      if (screen->xform&PS_BLUEPRINT_XFORM_HORZ) poi->x=PS_BLUEPRINT_COLC-poi->x-1;
      if (screen->xform&PS_BLUEPRINT_XFORM_VERT) poi->y=PS_BLUEPRINT_ROWC-poi->y-1;
      poi->x+=2;
      poi->y+=2;
    }
  }

  return 0;
}

/* Given a full row or column from a grid, locate the widest open space.
 * Do not examine the two cells on each end.
 */

static int ps_screen_find_widest_open_row(int *p,const struct ps_grid_cell *src) {
  int widestp=0,widestc=0,srcp=2;
  while (srcp<PS_GRID_COLC-2) {
    if (ps_blueprint_cell_is_passable(src[srcp].physics)) {
      int subc=1;
      while ((srcp+subc<PS_GRID_COLC-2)&&ps_blueprint_cell_is_passable(src[srcp+subc].physics)) subc++;
      if (subc>widestc) {
        widestp=srcp;
        widestc=subc;
      }
      srcp+=subc;
    } else {
      srcp++;
    }
  }
  *p=widestp;
  return widestc;
}

static int ps_screen_find_widest_open_col(int *p,const struct ps_grid_cell *src) {
  int widestp=0,widestc=0,srcp=2;
  while (srcp<PS_GRID_ROWC-2) {
    if (ps_blueprint_cell_is_passable(src[srcp*PS_GRID_COLC].physics)) {
      int subc=1;
      while ((srcp+subc<PS_GRID_COLC-2)&&ps_blueprint_cell_is_passable(src[(srcp+subc)*PS_GRID_COLC].physics)) subc++;
      if (subc>widestc) {
        widestp=srcp;
        widestc=subc;
      }
      srcp+=subc;
    } else {
      srcp++;
    }
  }
  *p=widestp;
  return widestc;
}

/* Populate north margin with a reference row.
 */

static int ps_screen_populate_grid_north_margin(struct ps_screen *screen,const struct ps_grid_cell *ref,int direct) {

  int refp,refc;
  refc=ps_screen_find_widest_open_row(&refp,ref);
  if (refc<1) return -1;

  int srcp,srcc;
  srcc=ps_screen_find_widest_open_row(&srcp,screen->grid->cellv+PS_GRID_COLC*2);
  if (srcc<1) return -1;

  if (!direct) {
    int dstp=(refp+srcp)>>1;
    int dstc=((refp+refc+srcp+srcc)>>1)-dstp;
    refp=dstp;
    refc=dstc;
  }

  ps_grid_set_physics(screen->grid,2,0,refp-2,1,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,refp+refc,0,PS_GRID_COLC-2-refc-refp,1,PS_BLUEPRINT_CELL_SOLID);

  refc=(refp+refc+srcp+srcc)>>1;
  refp=(refp+srcp)>>1;
  refc-=refp;

  ps_grid_set_physics(screen->grid,2,1,refp-2,1,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,refp+refc,1,PS_GRID_COLC-2-refc-refp,1,PS_BLUEPRINT_CELL_SOLID);

  return 0;
}

/* Populate south margin with a reference row.
 */

static int ps_screen_populate_grid_south_margin(struct ps_screen *screen,const struct ps_grid_cell *ref,int direct) {

  int refp,refc;
  refc=ps_screen_find_widest_open_row(&refp,ref);
  if (refc<1) return -1;

  int srcp,srcc;
  srcc=ps_screen_find_widest_open_row(&srcp,screen->grid->cellv+PS_GRID_SIZE-PS_GRID_COLC*3);
  if (srcc<1) return -1;

  if (!direct) {
    int dstp=(refp+srcp)>>1;
    int dstc=((refp+refc+srcp+srcc)>>1)-dstp;
    refp=dstp;
    refc=dstc;
  }

  ps_grid_set_physics(screen->grid,2,PS_GRID_ROWC-1,refp-2,1,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,refp+refc,PS_GRID_ROWC-1,PS_GRID_COLC-2-refc-refp,1,PS_BLUEPRINT_CELL_SOLID);

  refc=(refp+refc+srcp+srcc)>>1;
  refp=(refp+srcp)>>1;
  refc-=refp;

  ps_grid_set_physics(screen->grid,2,PS_GRID_ROWC-2,refp-2,1,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,refp+refc,PS_GRID_ROWC-2,PS_GRID_COLC-2-refc-refp,1,PS_BLUEPRINT_CELL_SOLID);

  return 0;
}

/* Populate west margin with a reference row.
 */

static int ps_screen_populate_grid_west_margin(struct ps_screen *screen,const struct ps_grid_cell *ref,int direct) {

  int refp,refc;
  refc=ps_screen_find_widest_open_col(&refp,ref);
  if (refc<1) return -1;

  int srcp,srcc;
  srcc=ps_screen_find_widest_open_col(&srcp,screen->grid->cellv+2);
  if (srcc<1) return -1;

  if (!direct) {
    int dstp=(refp+srcp)>>1;
    int dstc=((refp+refc+srcp+srcc)>>1)-dstp;
    refp=dstp;
    refc=dstc;
  }

  ps_grid_set_physics(screen->grid,0,2,1,refp-2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,0,refp+refc,1,PS_GRID_ROWC-2-refc-refp,PS_BLUEPRINT_CELL_SOLID);

  refc=(refp+refc+srcp+srcc)>>1;
  refp=(refp+srcp)>>1;
  refc-=refp;

  ps_grid_set_physics(screen->grid,1,2,1,refp-2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,1,refp+refc,1,PS_GRID_ROWC-2-refc-refp,PS_BLUEPRINT_CELL_SOLID);

  return 0;
}

/* Populate east margin with a reference row.
 */

static int ps_screen_populate_grid_east_margin(struct ps_screen *screen,const struct ps_grid_cell *ref,int direct) {

  int refp,refc;
  refc=ps_screen_find_widest_open_col(&refp,ref);
  if (refc<1) return -1;

  int srcp,srcc;
  srcc=ps_screen_find_widest_open_col(&srcp,screen->grid->cellv+PS_GRID_COLC-3);
  if (srcc<1) return -1;

  if (!direct) {
    int dstp=(refp+srcp)>>1;
    int dstc=((refp+refc+srcp+srcc)>>1)-dstp;
    refp=dstp;
    refc=dstc;
  }

  ps_grid_set_physics(screen->grid,PS_GRID_COLC-1,2,1,refp-2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,PS_GRID_COLC-1,refp+refc,1,PS_GRID_ROWC-2-refc-refp,PS_BLUEPRINT_CELL_SOLID);

  refc=(refp+refc+srcp+srcc)>>1;
  refp=(refp+srcp)>>1;
  refc-=refp;

  ps_grid_set_physics(screen->grid,PS_GRID_COLC-2,2,1,refp-2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,PS_GRID_COLC-2,refp+refc,1,PS_GRID_ROWC-2-refc-refp,PS_BLUEPRINT_CELL_SOLID);

  return 0;
}

/* Populate grid margins.
 * There are three possibilities on each edge:
 *   - No neighbor: Fill that margin solid.
 *   - Neighbor's grid not populated: Build a tunnel to halfway between my door and its.
 *   - Neighbor's grid populated: Build a tunnel to whatever it has open on the edge.
 * We can test whether the neighbor is populated by looking at its first cell.
 * That's always SOLID in a finished grid.
 */
int ps_screen_populate_grid_margins(struct ps_screen *screen,struct ps_grid *north,struct ps_grid *south,struct ps_grid *west,struct ps_grid *east) {
  if (!screen) return -1;
  if (!screen->grid) return -1;

  /* Always set SOLID the 16 corner cells (2x2 in each). */
  ps_grid_set_physics(screen->grid,0,0,2,2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,PS_GRID_COLC-2,0,2,2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,0,PS_GRID_ROWC-2,2,2,PS_BLUEPRINT_CELL_SOLID);
  ps_grid_set_physics(screen->grid,PS_GRID_COLC-2,PS_GRID_ROWC-2,2,2,PS_BLUEPRINT_CELL_SOLID);

  if (north) {
    if (north->cellv[0].physics) {
      if (ps_screen_populate_grid_north_margin(screen,north->cellv+PS_GRID_SIZE-PS_GRID_COLC,1)<0) return -1;
    } else {
      if (ps_screen_populate_grid_north_margin(screen,north->cellv+(PS_GRID_COLC*(PS_GRID_ROWC-3)),0)<0) return -1;
    }
  } else {
    if (ps_grid_set_physics(screen->grid,2,0,PS_GRID_COLC-4,2,PS_BLUEPRINT_CELL_SOLID)<0) return -1;
  }

  if (south) {
    if (south->cellv[0].physics) {
      if (ps_screen_populate_grid_south_margin(screen,south->cellv,1)<0) return -1;
    } else {
      if (ps_screen_populate_grid_south_margin(screen,south->cellv+PS_GRID_COLC*2,0)<0) return -1;
    }
  } else {
    if (ps_grid_set_physics(screen->grid,2,PS_GRID_ROWC-2,PS_GRID_COLC-4,2,PS_BLUEPRINT_CELL_SOLID)<0) return -1;
  }

  if (west) {
    if (west->cellv[0].physics) {
      if (ps_screen_populate_grid_west_margin(screen,west->cellv+PS_GRID_COLC-1,1)<0) return -1;
    } else {
      if (ps_screen_populate_grid_west_margin(screen,west->cellv+PS_GRID_COLC-3,0)<0) return -1;
    }
  } else {
    if (ps_grid_set_physics(screen->grid,0,2,2,PS_GRID_ROWC-4,PS_BLUEPRINT_CELL_SOLID)<0) return -1;
  }

  if (east) {
    if (east->cellv[0].physics) {
      if (ps_screen_populate_grid_east_margin(screen,east->cellv,1)<0) return -1;
    } else {
      if (ps_screen_populate_grid_east_margin(screen,east->cellv+2,0)<0) return -1;
    }
  } else {
    if (ps_grid_set_physics(screen->grid,PS_GRID_COLC-2,2,2,PS_GRID_ROWC-4,PS_BLUEPRINT_CELL_SOLID)<0) return -1;
  }

  return 0;
}
