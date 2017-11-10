#include "ps.h"
#include "ps_scgen.h"
#include "ps_scenario.h"
#include "ps_world.h"
#include "ps_screen.h"
#include "ps_region.h"
#include "ps_grid.h"
#include "ps_zone.h"
#include "ps_blueprint.h"

/* Initialize grid for skinning.
 */

static int ps_gridgen_initialize(struct ps_scgen *scgen,struct ps_screen *screen) {

  screen->grid->region=screen->region;
  screen->grid->monsterc_min=screen->blueprint->monsterc_min;
  screen->grid->monsterc_max=screen->blueprint->monsterc_max;

  int cellc=PS_GRID_SIZE;
  struct ps_grid_cell *cell=screen->grid->cellv;
  for (;cellc-->0;cell++) {
    cell->tsid=screen->region->tsid;
    cell->shape=0;
  }
  
  return 0;
}

/* Choose a shape from list, of a given style.
 * If there's more than one candidate, select randomly.
 */

static const struct ps_region_shape *ps_gridgen_select_shape_of_style(
  struct ps_scgen *scgen,const struct ps_region_shape *shapev,int shapec,uint8_t style
) {
  int candidatec=0,i,p;
  for (i=0;i<shapec;i++) if (shapev[i].style==style) {
    candidatec++;
    p=i;
  }
  if (candidatec<1) return 0;
  if (candidatec==1) return shapev+p; // Likely enough to warrant this ugly optimization.
  int selection=ps_scgen_randint(scgen,candidatec);
  for (i=0;i<shapec;i++) if (shapev[i].style==style) {
    if (!selection--) return shapev+i;
  }
  return shapev+p; // Impossible, but we handle it safely.
}

/* Choose a shape from the list, considering only those shapes which do not require combining.
 * This is most shapes.
 * FAT is a special case: It generally is universal, but some zone layouts are not compatible.
 */

static int ps_gridgen_shape_is_universal(const struct ps_region_shape *shape,int include_fat,int include_skinny) {
  switch (shape->style) {
    case PS_REGION_SHAPE_STYLE_SINGLE:
    case PS_REGION_SHAPE_STYLE_ALT4:
    case PS_REGION_SHAPE_STYLE_ALT8:
    case PS_REGION_SHAPE_STYLE_EVEN4:
    case PS_REGION_SHAPE_STYLE_ALT16:
      return 1;
    case PS_REGION_SHAPE_STYLE_FAT:
      return include_fat;
    case PS_REGION_SHAPE_STYLE_SKINNY:
      return include_skinny;
  }
  return 0;
}

static const struct ps_region_shape *ps_gridgen_select_universal_shape(
  struct ps_scgen *scgen,const struct ps_region_shape *shapev,int shapec,int include_fat,int include_skinny
) {
  int candidatec=0,i,p;
  for (i=0;i<shapec;i++) if (ps_gridgen_shape_is_universal(shapev+i,include_fat,include_skinny)) {
    candidatec++;
    p=i;
  }
  if (candidatec<1) return 0;
  if (candidatec==1) return shapev+p; // Likely enough to warrant this ugly optimization.
  int selection=ps_scgen_randint(scgen,candidatec);
  for (i=0;i<shapec;i++) if (ps_gridgen_shape_is_universal(shapev+i,include_fat,include_skinny)) {
    if (!selection--) return shapev+i;
  }
  return shapev+p; // Impossible, but we handle it safely.
}

/* Check if one or more of a given style is present.
 */

static int ps_gridgen_shapes_contain_style(const struct ps_region_shape *shapev,int shapec,uint8_t style) {
  for (;shapec-->0;shapev++) if (shapev->style==style) return 1;
  return 0;
}

/* Select a shape for each zone.
 * At this point, it is still OK to split zones.
 * Fail with a helpful error message if the available shapes can't accomodate us.
 */

static int ps_gridgen_select_shape_for_zone(struct ps_scgen *scgen,struct ps_screen *screen,struct ps_zone *zone) {

  const struct ps_region_shape *shapev;
  int shapec=ps_region_get_shapes(&shapev,screen->region,zone->physics);
  if (shapec<1) {
    ps_log(GENERATOR,ERROR,"No shapes for physics %d, tsid=%d.",zone->physics,screen->region->tsid);
    return -1;
  }

  if (ps_zone_analyze(zone)<0) return -1;

  /* If the physics is VACANT and we have an ALT16, use it.
   * Regions should declare only one VACANT shape, of style ALT16.
   */
  if (zone->physics==PS_BLUEPRINT_CELL_VACANT) {
    if (zone->shape=ps_gridgen_select_shape_of_style(scgen,shapev,shapec,PS_REGION_SHAPE_STYLE_ALT16)) {
      return 0;
    }
  }

  /* If the zone is an exact 3x3 and we have a 3X3 shape, use it. */
  if (zone->exact3x3) {
    if (zone->shape=ps_gridgen_select_shape_of_style(scgen,shapev,shapec,PS_REGION_SHAPE_STYLE_3X3)) {
      return 0;
    }
  }

  /* If we contain foursquares and are FAT-compatible and a FAT shape exists, use it. */
  if (zone->foursquarec&&!zone->fatfailc) {
    if (zone->shape=ps_gridgen_select_shape_of_style(scgen,shapev,shapec,PS_REGION_SHAPE_STYLE_FAT)) {
      return 0;
    }
  }

  /* If we have no foursquares, are larger than one cell, and a SKINNY shape exists, use it. */
  if (!zone->foursquarec&&(zone->cellc>1)) {
    if (zone->shape=ps_gridgen_select_shape_of_style(scgen,shapev,shapec,PS_REGION_SHAPE_STYLE_SKINNY)) {
      return 0;
    }
  }

  /* If we have foursquares and a FAT shape exists, split the zone. */
  if (zone->foursquarec&&ps_gridgen_shapes_contain_style(shapev,shapec,PS_REGION_SHAPE_STYLE_FAT)) {
    if (ps_zone_force_FAT_compatibility(zone,scgen->zones)<0) return -1;
    if (ps_zone_analyze(zone)<0) return -1;
    if (zone->shape=ps_gridgen_select_shape_of_style(scgen,shapev,shapec,PS_REGION_SHAPE_STYLE_FAT)) {
      return 0;
    }
  }

  // TODO: Consider splitting down to embedded 3X3
  if (zone->contains3x3) {
    ps_log(GENERATOR,DEBUG,"(TODO) Opportunity to split down to 3X3 for physics %d, tsid=%d.",zone->physics,screen->region->tsid);
  }

  /* Done with special logic. Pick any universal shape. */
  if (!(zone->shape=ps_gridgen_select_universal_shape(scgen,shapev,shapec,!zone->fatfailc,!zone->foursquarec))) {
    ps_log(GENERATOR,ERROR,"No universal shapes for physics %d, tsid=%d.",zone->physics,screen->region->tsid);
    return -1;
  }

  return 0;
}

static int ps_gridgen_select_shapes_for_zones(struct ps_scgen *scgen,struct ps_screen *screen) {
  int i=0; for (;i<scgen->zones->zonec;i++) {
    struct ps_zone *zone=scgen->zones->zonev[i];
    if (!zone->cellc) break; // zones->zonev typically has a few empties at the end.
    if (ps_gridgen_select_shape_for_zone(scgen,screen,zone)<0) return -1;
  }
  return 0;
}

/* Get tileid for specific shapes with randomization and neighbors.
 */

static uint8_t ps_gridgen_skin_cell_ALT4(struct ps_scgen *scgen,uint8_t tileid) {
  int bits=ps_scgen_randint(scgen,8);
       if (bits&0x01) return tileid+0;
  else if (bits&0x02) return tileid+1;
  else if (bits&0x04) return tileid+2;
  else                return tileid+3;
}

static uint8_t ps_gridgen_skin_cell_ALT8(struct ps_scgen *scgen,uint8_t tileid) {
  int bits=ps_scgen_randint(scgen,128);
       if (bits&0x01) return tileid+0x00;
  else if (bits&0x02) return tileid+0x01;
  else if (bits&0x04) return tileid+0x02;
  else if (bits&0x08) return tileid+0x03;
  else if (bits&0x10) return tileid+0x10;
  else if (bits&0x20) return tileid+0x11;
  else if (bits&0x40) return tileid+0x12;
  else                return tileid+0x13;
}

static uint8_t ps_gridgen_skin_cell_EVEN4(struct ps_scgen *scgen,uint8_t tileid) {
  return tileid+ps_scgen_randint(scgen,4);
}

static uint8_t ps_gridgen_skin_cell_ALT16(struct ps_scgen *scgen,uint8_t tileid) {
  int bits=ps_scgen_randint(scgen,2048);

  // (7/8): Select from top row evenly.
  if (bits&0x07) return tileid+((bits>>3)&0x03);

  // (1/16): Select from second row evenly.
  if (bits&0x08) return tileid+0x10+((bits>>4)&0x03);

  // (1/16): Bottom two rows are decreasingly likely by powers of two.
  // The last two tiles are both vanishingly rare (1/2048).
  if (bits&0x010) return tileid+0x20;
  if (bits&0x020) return tileid+0x21;
  if (bits&0x040) return tileid+0x22;
  if (bits&0x080) return tileid+0x23;
  if (bits&0x100) return tileid+0x30;
  if (bits&0x200) return tileid+0x31;
  if (bits&0x400) return tileid+0x32;
  return tileid+0x33;
}

#define NW 0x80
#define N  0x40
#define NE 0x20
#define W  0x10
#define E  0x08
#define SW 0x04
#define S  0x02
#define SE 0x01

#define MATCH(n) ((neighbors&(n))==(n))

static uint8_t ps_gridgen_skin_cell_SKINNY(uint8_t neighbors,uint8_t tileid) {
  if (MATCH(N|S|E|W)) return tileid+0x11;
  if (MATCH(W|E|S)) return tileid+0x01;
  if (MATCH(N|E|S)) return tileid+0x10;
  if (MATCH(N|W|S)) return tileid+0x12;
  if (MATCH(N|W|E)) return tileid+0x21;
  if (MATCH(E|S)) return tileid+0x00;
  if (MATCH(W|S)) return tileid+0x02;
  if (MATCH(N|S)) return tileid+0x13;
  if (MATCH(N|E)) return tileid+0x20;
  if (MATCH(N|W)) return tileid+0x22;
  if (MATCH(E|W)) return tileid+0x31;
  if (MATCH(S)) return tileid+0x03;
  if (MATCH(N)) return tileid+0x23;
  if (MATCH(E)) return tileid+0x30;
  if (MATCH(W)) return tileid+0x32;
  return tileid+0x33;
}

static uint8_t ps_gridgen_skin_cell_FAT(uint8_t neighbors,uint8_t tileid) {
  if (MATCH(NW|N|NE|W|E|SW|S|SE)) return tileid+0x11;
  if (MATCH(NW|N|NE|W|E|SW|S)) return tileid+0x03;
  if (MATCH(NW|N|NE|W|E|S|SE)) return tileid+0x04;
  if (MATCH(NW|N|W|E|SW|S|SE)) return tileid+0x13;
  if (MATCH(N|NE|W|E|SW|S|SE)) return tileid+0x14;
  if (MATCH(W|E|SW|S|SE)) return tileid+0x01;
  if (MATCH(N|NE|E|S|SE)) return tileid+0x10;
  if (MATCH(NW|N|W|SW|S)) return tileid+0x12;
  if (MATCH(NW|N|NE|W|E)) return tileid+0x21;
  if (MATCH(E|S|SE)) return tileid+0x00;
  if (MATCH(W|SW|S)) return tileid+0x02;
  if (MATCH(N|NE|E)) return tileid+0x20;
  if (MATCH(NW|N|W)) return tileid+0x22;
  return tileid+0x23;
}

static uint8_t ps_gridgen_skin_cell_3X3(uint8_t neighbors,uint8_t tileid) {
  if (MATCH(NW|N|NE|W|E|SW|S|SE)) return tileid+0x11;
  if (MATCH(W|E|SW|S|SE)) return tileid+0x01;
  if (MATCH(N|NE|E|S|SE)) return tileid+0x10;
  if (MATCH(NW|N|W|SW|S)) return tileid+0x12;
  if (MATCH(NW|N|NE|W|E)) return tileid+0x21;
  if (MATCH(E|S|SE)) return tileid+0x00;
  if (MATCH(W|SW|S)) return tileid+0x02;
  if (MATCH(N|NE|E)) return tileid+0x20;
  if (MATCH(NW|N|W)) return tileid+0x22;
  ps_log(GENERATOR,ERROR,"Inappropriate neighbor mask 0x%02x for 3X3 shape.",neighbors);
  return tileid+0x11; // Not actually appropriate; 3X3 does not have a singleton option.
}

#undef NW
#undef N
#undef NE
#undef W
#undef E
#undef SW
#undef S
#undef SE

#undef MATCH

/* Get cell shape. TODO This should be stored in the tilesheet resource.
 * Which is tricky, because that is a plain image file.
 * We should have a new resource type 'tsshape', which assigns itself into tilesheet at link time.
 * For now (early development), we're hard-coding the one image in use.
 */

static uint8_t ps_gridgen_shape_for_tile(uint8_t tsid,uint8_t tileid) {
  switch (tileid) {
    case 0x45: return PS_GRID_CELL_SHAPE_RNW;
    case 0x47: return PS_GRID_CELL_SHAPE_RNE;
    case 0x50: return PS_GRID_CELL_SHAPE_CIRCLE;
    case 0x60: return PS_GRID_CELL_SHAPE_CIRCLE;
    case 0x65: return PS_GRID_CELL_SHAPE_RSW;
    case 0x67: return PS_GRID_CELL_SHAPE_RSE;
    case 0x68: return PS_GRID_CELL_SHAPE_CIRCLE;
    case 0x70: return PS_GRID_CELL_SHAPE_CIRCLE;
    //TODO water cells, didn't bother
  }
  return 0;
}

/* Apply graphics.
 * By this point, the zones are final.
 */

static uint8_t ps_gridgen_skin_cell(struct ps_scgen *scgen,const struct ps_region_shape *shape,uint8_t mask) {
  switch (shape->style) {
    case PS_REGION_SHAPE_STYLE_SINGLE: return shape->tileid;
    case PS_REGION_SHAPE_STYLE_ALT4: return ps_gridgen_skin_cell_ALT4(scgen,shape->tileid);
    case PS_REGION_SHAPE_STYLE_ALT8: return ps_gridgen_skin_cell_ALT8(scgen,shape->tileid);
    case PS_REGION_SHAPE_STYLE_EVEN4: return ps_gridgen_skin_cell_EVEN4(scgen,shape->tileid);
    case PS_REGION_SHAPE_STYLE_ALT16: return ps_gridgen_skin_cell_ALT16(scgen,shape->tileid);
    case PS_REGION_SHAPE_STYLE_SKINNY: return ps_gridgen_skin_cell_SKINNY(mask,shape->tileid);
    case PS_REGION_SHAPE_STYLE_FAT: return ps_gridgen_skin_cell_FAT(mask,shape->tileid);
    case PS_REGION_SHAPE_STYLE_3X3: return ps_gridgen_skin_cell_3X3(mask,shape->tileid);
  }
  ps_log(GENERATOR,ERROR,"Failed to skin cell with shape style %d.",shape->style);
  return 0;
}

static int ps_gridgen_skin_cells_for_zone(struct ps_scgen *scgen,struct ps_screen *screen,struct ps_zone *zone) {
  const struct ps_zone_cell *zcell=zone->cellv;
  int i=zone->cellc; for (;i-->0;zcell++) {
    struct ps_grid_cell *gcell=screen->grid->cellv+zcell->y*PS_GRID_COLC+zcell->x;
    gcell->tileid=ps_gridgen_skin_cell(scgen,zone->shape,zcell->neighbors);
    gcell->shape=ps_gridgen_shape_for_tile(screen->region->tsid,gcell->tileid);
  }
  return 0;
}

static int ps_gridgen_skin_cells(struct ps_scgen *scgen,struct ps_screen *screen) {
  int i=0; for (;i<scgen->zones->zonec;i++) {
    if (ps_gridgen_skin_cells_for_zone(scgen,screen,scgen->zones->zonev[i])<0) return -1;
  }
  return 0;
}

/* Generate grid for one screen.
 */

static int ps_scgen_generate_grid(struct ps_scgen *scgen,struct ps_screen *screen) {
  if (!screen->region) return -1;
  if (!screen->blueprint) return -1;
  if (!screen->grid) return -1;

  if (ps_gridgen_initialize(scgen,screen)<0) return -1;
  if (ps_zones_rebuild(scgen->zones,screen->grid)<0) return -1;

  if (ps_gridgen_select_shapes_for_zones(scgen,screen)<0) return -1;

  if (ps_gridgen_skin_cells(scgen,screen)<0) return -1;

  return 0;
}

/* Generate grids, main entry point.
 * Every screen at this point has a blueprint, region, and grid.
 * The grid initially only has physics populated.
 */

int ps_scgen_generate_grids(struct ps_scgen *scgen) {
  if (!scgen) return -1;
  struct ps_world *world=scgen->scenario->world;
  int screenc=world->w*world->h;
  struct ps_screen *screen=world->v;
  for (;screenc-->0;screen++) {
    if (ps_scgen_generate_grid(scgen,screen)<0) return -1;
  }
  return 0;
}
