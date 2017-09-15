#include "ps.h"
#include "ps_grid.h"
#include "ps_blueprint.h"

/* New.
 */

struct ps_grid *ps_grid_new() {
  struct ps_grid *grid=calloc(1,sizeof(struct ps_grid));
  if (!grid) return 0;

  grid->refc=1;

  return grid;
}

/* Delete.
 */

void ps_grid_del(struct ps_grid *grid) {
  if (!grid) return;
  if (grid->refc-->1) return;

  if (grid->poiv) free(grid->poiv);

  free(grid);
}

/* Retain.
 */

int ps_grid_ref(struct ps_grid *grid) {
  if (!grid) return -1;
  if (grid->refc<1) return -1;
  if (grid->refc==INT_MAX) return -1;
  grid->refc++;
  return 0;
}

/* Set physics of all cells in a given rect.
 */

int ps_grid_set_physics(struct ps_grid *grid,int x,int y,int w,int h,uint8_t physics) {
  if (!grid) return -1;
  if ((w<1)||(h<1)) return 0;
  if ((x<0)||(y<0)||(x>PS_GRID_COLC-w)||(y>PS_GRID_ROWC-h)) return -1;

  struct ps_grid_cell *cell=grid->cellv+y*PS_GRID_COLC+x;
  while (h-->0) {
    int i=w; for (;i-->0;) cell[i].physics=physics;
    cell+=PS_GRID_COLC;
  }

  return 0;
}

/* Change barrier states.
 */
 
int ps_grid_open_barrier(struct ps_grid *grid,int barrierid) {
  if (!grid) return -1;
  //ps_log(GAME,TRACE,"%s %d",__func__,barrierid);
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    if (poi->type!=PS_BLUEPRINT_POI_BARRIER) continue;
    if (poi->argv[0]!=barrierid) continue;
    int cellp=poi->y*PS_GRID_COLC+poi->x;
    if (!grid->cellv[cellp].tileid) continue; // Already open.
    poi->argv[1]=grid->cellv[cellp].tileid;
    poi->argv[2]=grid->cellv[cellp].physics;
    grid->cellv[cellp].tileid=0;
    grid->cellv[cellp].physics=0;
  }
  return 0;
}

int ps_grid_close_barrier(struct ps_grid *grid,int barrierid) {
  if (!grid) return -1;
  //ps_log(GAME,TRACE,"%s %d",__func__,barrierid);
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    if (poi->type!=PS_BLUEPRINT_POI_BARRIER) continue;
    if (poi->argv[0]!=barrierid) continue;
    int cellp=poi->y*PS_GRID_COLC+poi->x;
    if (grid->cellv[cellp].tileid) continue; // Already closed.
    grid->cellv[cellp].tileid=poi->argv[1];
    grid->cellv[cellp].physics=poi->argv[2];
  }
  return 0;
}

int ps_grid_close_all_barriers(struct ps_grid *grid) {
  if (!grid) return -1;
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_BARRIER) {
      int cellp=poi->y*PS_GRID_COLC+poi->x;
      if (grid->cellv[cellp].tileid) {
        // Already closed.
      } else {
        grid->cellv[cellp].tileid=poi->argv[1];
        grid->cellv[cellp].physics=poi->argv[2];
      }
    }
  }
  return 0;
}
