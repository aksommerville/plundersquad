#include "ps.h"
#include "ps_zone.h"
#include "ps_grid.h"
#include "ps_blueprint.h"

/* Zone lifecycle.
 */

struct ps_zone *ps_zone_new() {
  struct ps_zone *zone=calloc(1,sizeof(struct ps_zone));
  if (!zone) return 0;
  return zone;
}

void ps_zone_del(struct ps_zone *zone) {
  if (!zone) return;
  if (zone->cellv) free(zone->cellv);
  free(zone);
}

/* Zone cell list, private primitives.
 */

// Nonstatic. Shared secretly by ps_zone_analyze.c
int ps_zone_search_cell(const struct ps_zone *zone,int x,int y) {
  int lo=0,hi=zone->cellc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (x<zone->cellv[ck].x) hi=ck;
    else if (x>zone->cellv[ck].x) lo=ck+1;
    else if (y<zone->cellv[ck].y) hi=ck;
    else if (y>zone->cellv[ck].y) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_zone_insert_cell(struct ps_zone *zone,int p,int x,int y) {
  if ((x<0)||(x>=PS_GRID_COLC)) return -1;
  if ((y<0)||(y>=PS_GRID_ROWC)) return -1;

  if (zone->cellc>=zone->cella) {
    int na=zone->cella+8;
    if (na>INT_MAX/sizeof(struct ps_zone_cell)) return -1;
    void *nv=realloc(zone->cellv,sizeof(struct ps_zone_cell)*na);
    if (!nv) return -1;
    zone->cellv=nv;
    zone->cella=na;
  }

  struct ps_zone_cell *cell=zone->cellv+p;
  memmove(cell+1,cell,sizeof(struct ps_zone_cell)*(zone->cellc-p));
  zone->cellc++;
  cell->x=x;
  cell->y=y;
  cell->neighbors=0;

  return 0;
}

/* Zone cell list, public functions.
 */

int ps_zone_has_cell(const struct ps_zone *zone,int x,int y) {
  if (!zone) return -1;

  // (x) OOB is the worst-case scenario for search. Pick these out directly.
  if ((x<0)||(x>=PS_GRID_COLC)) return 0;
  if ((y<0)||(y>=PS_GRID_ROWC)) return 0;
  
  return (ps_zone_search_cell(zone,x,y)>=0)?1:0;
}

int ps_zone_add_cell(struct ps_zone *zone,int x,int y) {
  if (!zone) return -1;
  int p=ps_zone_search_cell(zone,x,y);
  if (p>=0) return 0;
  p=-p-1;
  if (ps_zone_insert_cell(zone,p,x,y)<0) return -1;
  return 1;
}

int ps_zone_remove_cell(struct ps_zone *zone,int x,int y) {
  if (!zone) return -1;
  int p=ps_zone_search_cell(zone,x,y);
  if (p<0) return 0;
  zone->cellc--;
  memmove(zone->cellv+p,zone->cellv+p+1,sizeof(struct ps_zone_cell)*(zone->cellc-p));
  return 1;
}

int ps_zone_clear(struct ps_zone *zone) {
  if (!zone) return -1;
  zone->cellc=0;
  return 0;
}

/* Zones lifecycle.
 */

struct ps_zones *ps_zones_new() {
  struct ps_zones *zones=calloc(1,sizeof(struct ps_zones));
  if (!zones) return 0;
  return zones;
}

void ps_zones_del(struct ps_zones *zones) {
  if (!zones) return;
  if (zones->zonev) {
    while (zones->zonec-->0) ps_zone_del(zones->zonev[zones->zonec]);
    free(zones->zonev);
  }
  free(zones);
}

/* Spawn a zone from a zone list.
 */

struct ps_zone *ps_zones_spawn_zone(struct ps_zones *zones) {
  if (!zones) return 0;

  /* If we contain any empty zones, return the first one. */
  int i=0; for (;i<zones->zonec;i++) {
    if (!zones->zonev[i]->cellc) {
      struct ps_zone *zone=zones->zonev[i];
      zone->physics=0;
      zone->shape=0;
      return zone;
    }
  }

  /* Create a fresh one and add it. */
  struct ps_zone *zone=ps_zone_new();
  if (!zone) return 0;
  if (ps_zones_add_zone(zones,zone)<0) {
    ps_zone_del(zone);
    return 0;
  }

  return zone;
}

/* Add a zone to zones.
 */
 
int ps_zones_add_zone(struct ps_zones *zones,struct ps_zone *zone_HANDOFF) {
  if (!zones||!zone_HANDOFF) return -1;
  int i; for (i=0;i<zones->zonec;i++) if (zones->zonev[i]==zone_HANDOFF) return -1;

  if (zones->zonec>=zones->zonea) {
    int na=zones->zonea+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(zones->zonev,sizeof(void*)*na);
    if (!nv) return -1;
    zones->zonev=nv;
    zones->zonea=na;
  }

  zones->zonev[zones->zonec++]=zone_HANDOFF;
  
  return 0;
}

/* Clear zones.
 * Don't actually delete zone objects. Just clear all of them.
 */
 
int ps_zones_clear(struct ps_zones *zones) {
  if (!zones) return -1;
  int i; for (i=0;i<zones->zonec;i++) {
    ps_zone_clear(zones->zonev[i]);
  }
  return 0;
}

/* Clear (cell.shape) for all cells in grid.
 */

static void ps_zones_wipe_grid_shape(struct ps_grid *grid) {
  struct ps_grid_cell *cell=grid->cellv;
  int i=PS_GRID_SIZE;
  for (;i-->0;cell++) {
    cell->shape=0;
  }
}

/* Seed fill one zone from grid.
 * Caller must check that this cell is appropriate.
 */

static int ps_zone_physics_match(uint8_t a,uint8_t b) {
  if (a==b) return 1;

  #define EQ(atag,btag) \
    if ((a==PS_BLUEPRINT_CELL_##atag)&&(b==PS_BLUEPRINT_CELL_##btag)) return 1; \
    if ((a==PS_BLUEPRINT_CELL_##btag)&&(b==PS_BLUEPRINT_CELL_##atag)) return 1;

  EQ(VACANT,HEROONLY)

  #undef EQ
  return 0;
}

static int ps_zone_seed_fill(struct ps_zone *zone,struct ps_grid *grid,int x,int y) {
  struct ps_grid_cell *cell=grid->cellv+y*PS_GRID_COLC+x;
  cell->shape=1;
  if (ps_zone_add_cell(zone,x,y)<0) return -1;
  struct ps_grid_cell *neighbor;
  if (x>0) {
    neighbor=cell-1;
    if (!neighbor->shape&&ps_zone_physics_match(neighbor->physics,cell->physics)) {
      if (ps_zone_seed_fill(zone,grid,x-1,y)<0) return -1;
    }
  }
  if (y>0) {
    neighbor=cell-PS_GRID_COLC;
    if (!neighbor->shape&&ps_zone_physics_match(neighbor->physics,cell->physics)) {
      if (ps_zone_seed_fill(zone,grid,x,y-1)<0) return -1;
    }
  }
  if (x<PS_GRID_COLC-1) {
    neighbor=cell+1;
    if (!neighbor->shape&&ps_zone_physics_match(neighbor->physics,cell->physics)) {
      if (ps_zone_seed_fill(zone,grid,x+1,y)<0) return -1;
    }
  }
  if (y<PS_GRID_ROWC-1) {
    neighbor=cell+PS_GRID_COLC;
    if (!neighbor->shape&&ps_zone_physics_match(neighbor->physics,cell->physics)) {
      if (ps_zone_seed_fill(zone,grid,x,y+1)<0) return -1;
    }
  }
  return 0;
}

/* Rebuild zones from grid.
 */

static uint8_t ps_zone_normalize_physics(uint8_t physics) {
  if (physics==PS_BLUEPRINT_CELL_HEROONLY) return PS_BLUEPRINT_CELL_VACANT;
  return physics;
}
 
int ps_zones_rebuild(struct ps_zones *zones,struct ps_grid *grid) {
  if (!zones||!grid) return -1;
  ps_zones_clear(zones);
  ps_zones_wipe_grid_shape(grid);

  struct ps_grid_cell *cell=grid->cellv;
  int y=0; for (;y<PS_GRID_ROWC;y++) {
    int x=0; for (;x<PS_GRID_COLC;x++,cell++) {
      if (cell->shape) continue;
      struct ps_zone *zone=ps_zones_spawn_zone(zones);
      if (!zone) return -1;
      zone->physics=ps_zone_normalize_physics(cell->physics);
      if (ps_zone_seed_fill(zone,grid,x,y)<0) return -1;
    }
  }

  ps_zones_wipe_grid_shape(grid);
  return 0;
}
