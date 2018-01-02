#include "ps.h"
#include "ps_zone.h"

int ps_zone_search_cell(const struct ps_zone *zone,int x,int y);

/* Reset neighbor masks.
 * OOB cells are assumed to match their nearest neighbor.
 */

static int ps_zone_has_clamped_cell(const struct ps_zone *zone,int x,int y) {
  if (x<0) x=0; else if (x>=PS_GRID_COLC) x=PS_GRID_COLC-1;
  if (y<0) y=0; else if (y>=PS_GRID_ROWC) y=PS_GRID_ROWC-1;
  return ps_zone_has_cell(zone,x,y);
}

static uint8_t ps_zone_calculate_neighbor_mask(const struct ps_zone *zone,int x,int y) {
  uint8_t mask=0;
  if (ps_zone_has_clamped_cell(zone,x-1,y-1)) mask|=0x80;
  if (ps_zone_has_clamped_cell(zone,x  ,y-1)) mask|=0x40;
  if (ps_zone_has_clamped_cell(zone,x+1,y-1)) mask|=0x20;
  if (ps_zone_has_clamped_cell(zone,x-1,y  )) mask|=0x10;
  if (ps_zone_has_clamped_cell(zone,x+1,y  )) mask|=0x08;
  if (ps_zone_has_clamped_cell(zone,x-1,y+1)) mask|=0x04;
  if (ps_zone_has_clamped_cell(zone,x  ,y+1)) mask|=0x02;
  if (ps_zone_has_clamped_cell(zone,x+1,y+1)) mask|=0x01;
  return mask;
}

static int ps_zone_reset_neighbor_masks(struct ps_zone *zone) {
  struct ps_zone_cell *cell=zone->cellv;
  int i=zone->cellc; for (;i-->0;cell++) {
    cell->neighbors=ps_zone_calculate_neighbor_mask(zone,cell->x,cell->y);
  }
  return 0;
}

/* Analyze one cell.
 * Record observations in (zone).
 */

static int ps_zone_analyze_cell(struct ps_zone *zone,const struct ps_zone_cell *cell) {

  /* Record a foursquare if we are the NW corner of it. */
  if ((cell->neighbors&0x0b)==0x0b) zone->foursquarec++;

  /* Record a ninesquare if we are the center of it. */
  if (cell->neighbors==0xff) zone->contains3x3++;

  /* Are we on an edge? Count each cell only once, even if it's a corner. */
  if (!cell->x||!cell->y||(cell->x==PS_GRID_COLC-1)||(cell->y==PS_GRID_ROWC-1)) zone->edgec++;

  /* Look for FAT shape compatibility.
   * We are FAT-compabtible if every cell is a member of a foursquare.
   * Later on, we might be able to force FAT compability by splitting.
   */
       if ((cell->neighbors&0xd0)==0xd0) ; // NW foursquare
  else if ((cell->neighbors&0x68)==0x68) ; // NE foursquare
  else if ((cell->neighbors&0x16)==0x16) ; // SW foursquare
  else if ((cell->neighbors&0x0b)==0x0b) ; // SE foursquare
  else if (!cell->neighbors) ; // Singleton; FAT is allowed.
  else zone->fatfailc++;

  return 0;
}

/* Analyze zone, main entry point.
 */

int ps_zone_analyze(struct ps_zone *zone) {
  if (!zone) return -1;

  /* Reset report. */
  zone->foursquarec=0;
  zone->edgec=0;
  zone->exact3x3=0;
  zone->contains3x3=0;
  zone->fatfailc=0;

  /* Reset neighbor masks. Most analysis depends on these. */
  if (ps_zone_reset_neighbor_masks(zone)<0) return -1;

  /* Run over each cell and count features. */
  const struct ps_zone_cell *cell=zone->cellv;
  int i=zone->cellc; for (;i-->0;cell++) {
    if (ps_zone_analyze_cell(zone,cell)<0) return -1;
  }

  /* exact3x3 is special. */
  if ((zone->cellc==9)&&zone->contains3x3) zone->exact3x3=1;

  return 0;
}

/* Add each cell from (zone) into one of (good,bad), based on FAT compatibility.
 */

static int ps_zone_cell_is_FAT_compatible(const struct ps_zone_cell *cell) {

  /* Member of a foursquare? Compatible. */
  if ((cell->neighbors&0xd0)==0xd0) return 1;
  if ((cell->neighbors&0x68)==0x68) return 1;
  if ((cell->neighbors&0x16)==0x16) return 1;
  if ((cell->neighbors&0x0b)==0x0b) return 1;
  
  return 0;
}

static int ps_zone_sort_cells_by_FAT_compatibility(struct ps_zone *good,struct ps_zone *bad,const struct ps_zone *zone) {
  const struct ps_zone_cell *cell=zone->cellv;
  int i=0; for (;i<zone->cellc;i++,cell++) {

    // There is a new, earlier pass that may drop cells into (bad) before this. Ignore those.
    if (ps_zone_has_cell(bad,cell->x,cell->y)) continue;
    
    if (ps_zone_cell_is_FAT_compatible(cell)) {
      if (ps_zone_add_cell(good,cell->x,cell->y)<0) return -1;
    } else {
      if (ps_zone_add_cell(bad,cell->x,cell->y)<0) return -1;
    }
  }
  if (!good->cellc) return -1;
  if (ps_zone_analyze(good)<0) return -1;
  if (ps_zone_analyze(bad)<0) return -1;
  return 0;
}

/* Transfer all contiguous cells from one zone to another.
 * We assume that (cell->neighbors) contains all neighbors, but may contain others.
 * Any transferred cell is removed from the source zone.
 * You provide the index of the first cell to transfer.
 */

static int ps_zone_cell_is_isthmus(const struct ps_zone_cell *cell) {
  if (cell->neighbors==0xdb) return 1;
  if (cell->neighbors==0x7e) return 1;
  return 0;
}

static int ps_zone_transfer_contiguous_cells(struct ps_zone *dst,struct ps_zone *src,int cellp) {
  if (!dst||!src) return -1;
  if ((cellp<0)||(cellp>=src->cellc)) return -1;
  int x=src->cellv[cellp].x;
  int y=src->cellv[cellp].y;
  uint8_t neighbors=src->cellv[cellp].neighbors;

  /* Add to destination. Note: neighbor mask is dropped in this transfer. */
  if (ps_zone_add_cell(dst,x,y)<1) return -1;

  /* Remove from source. Important to do this before recurring! */
  if (ps_zone_remove_cell(src,x,y)<1) return -1;

  /* Recur into cardinal neighbors. */
  #define RECUR(mask,dx,dy) \
    if (neighbors&mask) { \
      int p=ps_zone_search_cell(src,x+dx,y+dy); \
      if (p>=0) { \
        if (!ps_zone_cell_is_isthmus(src->cellv+p)) { \
          if (ps_zone_transfer_contiguous_cells(dst,src,p)<0) return -1; \
        } \
      } \
    }
  RECUR(0x40,0,-1)
  RECUR(0x10,-1,0)
  RECUR(0x08,1,0)
  RECUR(0x02,0,1)
  #undef RECUR

  return 0;
}

/* Locate isthmi and break them by adding to (bad) zone and altering neighbor masks.
 * An isthmus is a cell with opposite diagonal neighbors missing from the zone.
 * For example:
 *   +---+---+---+
 *   |   | X | X |
 *   +---+---+---+
 *   | X | X | X | <-- Our skins don't accomodate the middle cell.
 *   +---+---+---+
 *   | X | X |   |
 *   +---+---+---+
 * HYPOTHESIS: If a neighbor mask contains exactly 2 opposite diagonals, it is an isthmus.
 */

static int ps_zone_mask_is_isthmus(uint8_t neighbors) {
  // All we need is the diagonals.
  neighbors&=0xa5;
  if (neighbors==0x81) return 1;
  if (neighbors==0x24) return 1;
  return 0;
}

static void ps_zone_remove_cell_from_neighbors(struct ps_zone *zone,int x,int y) {
  uint8_t mask=1;
  int dy; for (dy=-1;dy<=1;dy++) {
    int dx; for (dx=-1;dx<=1;dx++) {
      if (!dx&&!dy) continue;
      int p=ps_zone_search_cell(zone,x+dx,y+dy);
      if (p>=0) {
        struct ps_zone_cell *cell=zone->cellv+p;
        cell->neighbors&=~mask;
      }
      mask<<=1;
    }
  }
}

static int ps_zone_break_isthmi(struct ps_zone *zone,struct ps_zone *bad) {
  int i=0; for (;i<zone->cellc;i++) {
    struct ps_zone_cell *cell=zone->cellv+i;
    if (!ps_zone_mask_is_isthmus(cell->neighbors)) continue;
    if (ps_zone_add_cell(bad,cell->x,cell->y)<0) return -1;
    cell->neighbors=0x00;
    ps_zone_remove_cell_from_neighbors(zone,cell->x,cell->y);
  }
  return 0;
}

/* Force FAT compatibility.
 * The basic strategy:
 *   - If we have an isthmus (see above), carefully remove it.
 *   - Visit each cell and sort them into two temporary zones: good (foursquare) and bad (skinny).
 *   - Identify any contiguous set from the good zone and replace ourself with that.
 *   - Spawn a new zone for each remaining contiguous 'good' set.
 *   - Spawn a new zone for each contiguous 'bad' set.
 * It is an error if the incoming zone has no embedded foursquares.
 * It is an error if the incoming zone has not been analzyed.
 */
 
int ps_zone_force_FAT_compatibility(struct ps_zone *zone,struct ps_zones *zones) {
  if (!zone||!zones) return -1;
  struct ps_zone *good=ps_zone_new();
  if (!good) return -1;
  struct ps_zone *bad=ps_zone_new();
  if (!bad) { ps_zone_del(good); return -1; }

  if (ps_zone_break_isthmi(zone,bad)<0) {
    ps_zone_del(good);
    ps_zone_del(bad);
    return -1;
  }

  if (ps_zone_sort_cells_by_FAT_compatibility(good,bad,zone)<0) {
    ps_zone_del(good);
    ps_zone_del(bad);
    return -1;
  }

  zone->cellc=0;

  if (ps_zone_transfer_contiguous_cells(zone,good,0)<0) {
    ps_zone_del(good);
    ps_zone_del(bad);
    return -1;
  }

  while (good->cellc>0) {
    struct ps_zone *nzone=ps_zones_spawn_zone(zones);
    nzone->physics=zone->physics;
    if (ps_zone_transfer_contiguous_cells(nzone,good,0)<0) {
      ps_zone_del(good);
      ps_zone_del(bad);
      return -1;
    }
  }
  while (bad->cellc>0) {
    struct ps_zone *nzone=ps_zones_spawn_zone(zones);
    nzone->physics=zone->physics;
    if (ps_zone_transfer_contiguous_cells(nzone,bad,0)<0) {
      ps_zone_del(good);
      ps_zone_del(bad);
      return -1;
    }
  }

  ps_zone_del(good);
  ps_zone_del(bad);
  return 0;
}
