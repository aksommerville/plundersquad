/* ps_grid.h
 * Single screen of map, composed by generator from a blueprint.
 */

#ifndef PS_GRID_H
#define PS_GRID_H

struct ps_blueprint_poi;
struct ps_region;

#define PS_GRID_CELL_SHAPE_RNW    0x01
#define PS_GRID_CELL_SHAPE_RNE    0x02
#define PS_GRID_CELL_SHAPE_RSW    0x04
#define PS_GRID_CELL_SHAPE_RSE    0x08
#define PS_GRID_CELL_SHAPE_CIRCLE 0x0f

struct ps_grid_cell {
  uint8_t tsid; // Resource ID of tilesheet. XXX all tiles must be from the same sheet
  uint8_t tileid; // Tile within sheet.
  uint8_t physics; // PS_BLUEPRINT_CELL_*
  uint8_t shape; // Used transiently during skinning. Once live, mask of PS_GRID_CELL_SHAPE_*. Zero default.
};

struct ps_grid {
  int refc;
  struct ps_blueprint_poi *poiv;
  int poic;
  struct ps_region *region; // WEAK, optional
  uint8_t monsterc_min,monsterc_max; // Copied from blueprint.
  struct ps_grid_cell cellv[PS_GRID_SIZE];
};

struct ps_grid *ps_grid_new();
void ps_grid_del(struct ps_grid *grid);
int ps_grid_ref(struct ps_grid *grid);

int ps_grid_set_physics(struct ps_grid *grid,int x,int y,int w,int h,uint8_t physics);

int ps_grid_open_barrier(struct ps_grid *grid,int barrierid);
int ps_grid_close_barrier(struct ps_grid *grid,int barrierid);
int ps_grid_close_all_barriers(struct ps_grid *grid);

#endif
