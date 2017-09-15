/* ps_zone.h
 * Contiguous region of cells in a grid with matching physics.
 * Dividing the grid into zones is a critical first step to skinning it.
 */

#ifndef PS_ZONE_H
#define PS_ZONE_H

struct ps_grid;
struct ps_region_shape;

struct ps_zone_cell {
  int x,y;
  uint8_t neighbors; // Populated at ps_zone_analyze().
};

struct ps_zone {
  uint8_t physics;
  const struct ps_region_shape *shape; // WEAK
  struct ps_zone_cell *cellv;
  int cellc,cella;

// Populated at ps_zone_analyze():
  int foursquarec; // How many foursquares? These make SKINNY unattractive.
  int edgec;       // How many edge cells? Not sure if this matters.
  int exact3x3;    // Nonzero only if this zone is a precise ninesquare.
  int contains3x3; // Nonzero if we contain at least one ninesquare, maybe you want to split it out.
  int fatfailc;    // Count of cells that would break FAT combining.
  
};

struct ps_zones {
  struct ps_zone **zonev;
  int zonec,zonea;
};

/* You generally don't allocate or free individual zones.
 */
struct ps_zone *ps_zone_new();
void ps_zone_del(struct ps_zone *zone);

int ps_zone_has_cell(const struct ps_zone *zone,int x,int y);
int ps_zone_add_cell(struct ps_zone *zone,int x,int y);
int ps_zone_remove_cell(struct ps_zone *zone,int x,int y);
int ps_zone_clear(struct ps_zone *zone);

/* Refresh our assessment of the zone's shape compatibility.
 * This is not done automatically.
 */
int ps_zone_analyze(struct ps_zone *zone);

struct ps_zones *ps_zones_new();
void ps_zones_del(struct ps_zones *zones);

/* zones may keep a few zombies to avoid excess allocation.
 * Use ps_zones_spawn_zone() as much as possible, to let us make the decisions.
 */
struct ps_zone *ps_zones_spawn_zone(struct ps_zones *zones);
int ps_zones_add_zone(struct ps_zones *zones,struct ps_zone *zone_HANDOFF);
int ps_zones_clear(struct ps_zones *zones);

/* Clear these zones and rebuild from scratch against (grid).
 * (grid) is effectively constant, but we do twiddle cellv[].reserved during play.
 */
int ps_zones_rebuild(struct ps_zones *zones,struct ps_grid *grid);

/* Reduce the size of the zone to guarantee that a FAT shape is compatible.
 * (ie every cell will be a member of a foursquare).
 * Any cells removed from the zone will be dropped into new zones.
 * You must re-analyze after doing this.
 */
int ps_zone_force_FAT_compatibility(struct ps_zone *zone,struct ps_zones *zones);

#endif
