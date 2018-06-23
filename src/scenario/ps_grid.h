/* ps_grid.h
 * Single screen of map, composed by generator from a blueprint.
 * Grid has a serial format, which is managed by ps_scenario.
 */

#ifndef PS_GRID_H
#define PS_GRID_H

struct ps_blueprint_poi;
struct ps_region;
struct ps_path;

/* Cell shape bits. */
#define PS_GRID_CELL_SHAPE_RNW    0x01
#define PS_GRID_CELL_SHAPE_RNE    0x02
#define PS_GRID_CELL_SHAPE_RSW    0x04
#define PS_GRID_CELL_SHAPE_RSE    0x08

/* Compound cell shapes. */
#define PS_GRID_CELL_SHAPE_SQUARE 0x00
#define PS_GRID_CELL_SHAPE_RN     0x03
#define PS_GRID_CELL_SHAPE_RS     0x0c
#define PS_GRID_CELL_SHAPE_RW     0x05
#define PS_GRID_CELL_SHAPE_RE     0x0a
#define PS_GRID_CELL_SHAPE_CIRCLE 0x0f

struct ps_grid_cell {
  uint8_t tileid; // Tile within sheet.
  uint8_t physics; // PS_BLUEPRINT_CELL_*
  uint8_t shape; // Used transiently during skinning. Once live, mask of PS_GRID_CELL_SHAPE_*. Zero default.
};

struct ps_grid {
  int refc;
  struct ps_blueprint_poi *poiv;
  int poic;
  struct ps_region *region; // WEAK
  uint8_t monsterc_min,monsterc_max; // Copied from blueprint.
  int visited; // Set by game first time player sees this grid.
  struct ps_grid_cell cellv[PS_GRID_SIZE];
};

struct ps_grid *ps_grid_new();
void ps_grid_del(struct ps_grid *grid);
int ps_grid_ref(struct ps_grid *grid);

int ps_grid_set_physics(struct ps_grid *grid,int x,int y,int w,int h,uint8_t physics);

/* Only ps_game should use these functions.
 * It has a broader concept of barrier, with its own API (ps_game_adjust_barrier()).
 * (path) is optional. If provided, we fill it with all cells changed.
 */
int ps_grid_open_barrier(struct ps_grid *grid,int barrierid,struct ps_path *path);
int ps_grid_close_barrier(struct ps_grid *grid,int barrierid,struct ps_path *path);
int ps_grid_close_all_barriers(struct ps_grid *grid);

/* Given a rectangle in pixels, is there any cell matching (impassable) within it?
 */
int ps_grid_test_rect_physics(const struct ps_grid *grid,int x,int y,int w,int h,uint16_t impassable);

static inline struct ps_grid_cell *ps_grid_get_cell(struct ps_grid *grid,int x,int y) {
  if (!grid) return 0;
  if (x<0) x=0;
  else if (x>=PS_GRID_COLC) x=PS_GRID_COLC;
  if (y<0) y=0;
  else if (y>=PS_GRID_ROWC) y=PS_GRID_ROWC;
  return grid->cellv+y*PS_GRID_COLC+x;
}

// 0=no, 1=yes, 2=yes if all switches are set
int ps_grid_should_persist_switch(const struct ps_grid *grid,int switchid);

#endif
