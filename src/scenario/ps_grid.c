#include "ps.h"
#include "ps_grid.h"
#include "ps_blueprint.h"
#include "game/ps_path.h"

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
 * "open" means value==1.
 * "closed" means value==0.
 * It's a little wonky because we now have BARRIER and REVBARRIER.
 */
 
int ps_grid_open_barrier(struct ps_grid *grid,int barrierid,struct ps_path *path) {
  if (!grid) return -1;
  //ps_log(GAME,TRACE,"%s %d",__func__,barrierid);
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    switch (poi->type) {
      case PS_BLUEPRINT_POI_BARRIER: {
          if (poi->argv[0]!=barrierid) continue;
          int cellp=poi->y*PS_GRID_COLC+poi->x;
          if (!grid->cellv[cellp].tileid) continue; // Already open.
          poi->argv[1]=grid->cellv[cellp].tileid;
          poi->argv[2]=grid->cellv[cellp].physics;
          grid->cellv[cellp].tileid=0;
          grid->cellv[cellp].physics=0;
          if (path&&(ps_path_add(path,poi->x,poi->y)<0)) return -1;
        } break;
      case PS_BLUEPRINT_POI_REVBARRIER: {
          if (poi->argv[0]!=barrierid) continue;
          int cellp=poi->y*PS_GRID_COLC+poi->x;
          if (grid->cellv[cellp].tileid) continue; // Already open.
          grid->cellv[cellp].tileid=poi->argv[1];
          grid->cellv[cellp].physics=poi->argv[2];
          if (path&&(ps_path_add(path,poi->x,poi->y)<0)) return -1;
        } break;
    }
  }
  return 0;
}

int ps_grid_close_barrier(struct ps_grid *grid,int barrierid,struct ps_path *path) {
  if (!grid) return -1;
  //ps_log(GAME,TRACE,"%s %d",__func__,barrierid);
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    switch (poi->type) {
      case PS_BLUEPRINT_POI_BARRIER: {
          if (poi->argv[0]!=barrierid) continue;
          int cellp=poi->y*PS_GRID_COLC+poi->x;
          if (grid->cellv[cellp].tileid) continue; // Already open.
          grid->cellv[cellp].tileid=poi->argv[1];
          grid->cellv[cellp].physics=poi->argv[2];
          if (path&&(ps_path_add(path,poi->x,poi->y)<0)) return -1;
        } break;
      case PS_BLUEPRINT_POI_REVBARRIER: {
          if (poi->argv[0]!=barrierid) continue;
          int cellp=poi->y*PS_GRID_COLC+poi->x;
          if (!grid->cellv[cellp].tileid) continue; // Already open.
          poi->argv[1]=grid->cellv[cellp].tileid;
          poi->argv[2]=grid->cellv[cellp].physics;
          grid->cellv[cellp].tileid=0;
          grid->cellv[cellp].physics=0;
          if (path&&(ps_path_add(path,poi->x,poi->y)<0)) return -1;
        } break;
    }
  }
  return 0;
}

int ps_grid_close_all_barriers(struct ps_grid *grid) {
  if (!grid) return -1;
  //ps_log(GAME,TRACE,"%s",__func__);
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
    } else if (poi->type==PS_BLUEPRINT_POI_REVBARRIER) {
      int cellp=poi->y*PS_GRID_COLC+poi->x;
      if (grid->cellv[cellp].tileid) {
        poi->argv[1]=grid->cellv[cellp].tileid;
        poi->argv[2]=grid->cellv[cellp].physics;
        grid->cellv[cellp].tileid=0;
        grid->cellv[cellp].physics=0;
      }
    }
  }
  return 0;
}

/* Test rectangle.
 */
 
int ps_grid_test_rect_physics(const struct ps_grid *grid,int x,int y,int w,int h,uint16_t impassable) {
  if (!grid) return 0;
  int cola=x/PS_TILESIZE; if (cola<0) cola=0;
  int rowa=y/PS_TILESIZE; if (rowa<0) rowa=0;
  int colz=(x+w-1)/PS_TILESIZE; if (colz>=PS_GRID_COLC) colz=PS_GRID_COLC-1;
  int rowz=(y+h-1)/PS_TILESIZE; if (rowz>=PS_GRID_ROWC) rowz=PS_GRID_ROWC-1;
  int colc=colz-cola+1;
  int rowc=rowz-rowa+1;

  const struct ps_grid_cell *row=grid->cellv+rowa*PS_GRID_COLC+cola;
  for (;rowc-->0;row+=PS_GRID_COLC) {
    const struct ps_grid_cell *cell=row;
    int i=colc; for (;i-->0;cell++) {
      if (impassable&(1<<cell->physics)) return 1;
    }
  }

  return 0;
}

/* Check if a switch should be persisted.
 */
 
int ps_grid_should_persist_switch(const struct ps_grid *grid,int switchid) {
  if (!grid) return 0;
  if (switchid<1) return 0;
  const struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_PERMASWITCH) {
      if (poi->argv[0]==PS_PERMASWITCH_ALL_SWITCHES) return 2;
      if (poi->argv[0]==switchid) return 1;
    }
  }
  return 0;
}
