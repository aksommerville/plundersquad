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

/* Count POI.
 */
 
int ps_grid_count_poi_of_type(const struct ps_grid *grid,uint8_t type) {
  if (!grid) return 0;
  int c=0;
  const struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic;
  for (;i-->0;poi++) {
    if (poi->type==type) c++;
  }
  return c;
}

/* Trace line to detect physics.
 */
 
int ps_grid_line_contains_physics(const struct ps_grid *grid,int ax,int ay,int bx,int by,uint16_t phymask) {
  if (!grid) return 0;
  if (ax<0) ax=0; else if (ax>=PS_GRID_COLC) ax=PS_GRID_COLC-1;
  if (ay<0) ay=0; else if (ay>=PS_GRID_ROWC) ay=PS_GRID_ROWC-1;
  if (bx<0) bx=0; else if (bx>=PS_GRID_COLC) bx=PS_GRID_COLC-1;
  if (by<0) by=0; else if (by>=PS_GRID_ROWC) by=PS_GRID_ROWC-1;
  
  int dx=(ax<bx)?1:-1;
  int dy=(ay<by)?1:-1;
  int wx=bx-ax; if (wx<0) wx=-wx;
  int wy=by-ay; if (wy>0) wy=-wy;
  int xthresh=wx>>1;
  int ythresh=wy>>1;
  int weight=wx+wy;
  
  while (1) {
    if (phymask&(1<<grid->cellv[ay*PS_GRID_COLC+ax].physics)) return 1;
    if (weight>xthresh) {
      if (ax!=bx) ax+=dx;
      weight+=wy;
    } else if (weight<ythresh) {
      if (ay!=by) ay+=dy;
      weight+=wx;
    } else {
      if (ax!=bx) ax+=dx;
      if (ay!=by) ay+=dy;
      weight+=wx+wy;
    }
    if ((ax==bx)&&(ay==by)) return 0;
  }
}

/* Locate nearest neighbor matching physics.
 * We do this slick diamond rotation to deterministically identify the nearest by manhattan distance.
 * We start looking at the bottom of the diamond and proceed counterclockwise, returning the first match.
 */

int ps_grid_get_nearest_neighbor_matching_physics_1(
  int *dstcol,int *dstrow,const struct ps_grid *grid,int col,int row,uint16_t phymask,int distance
) {
  int dx=0,dy=distance,phase=0,valid=0;
  while (1) {
    int ckcol=col+dx;
    if ((ckcol>=0)&&(ckcol<PS_GRID_COLC)) {
      int ckrow=row+dy;
      if ((ckrow>=0)&&(ckrow<PS_GRID_ROWC)) {
        valid=1;
        if (phymask&(1<<grid->cellv[ckrow*PS_GRID_COLC+ckcol].physics)) {
          *dstcol=ckcol;
          *dstrow=ckrow;
          return 1;
        }
      }
    }
    switch (phase) {
      case 0: {
          dy--;
          dx++;
          if (!dy) phase=1;
        } break;
      case 1: {
          dy--;
          dx--;
          if (!dx) phase=2;
        } break;
      case 2: {
          dy++;
          dx--;
          if (!dy) phase=3;
        } break;
      case 3: {
          dy++;
          dx++;
          if (!dx) return valid?0:-1;
        } break;
    }
  }
}
 
int ps_grid_get_cardinal_neighbor_matching_physics(
  int *dstcol,int *dstrow,const struct ps_grid *grid,int col,int row,uint16_t phymask,int xpref,int ypref
) {
  if (!dstcol||!dstrow||!grid||!phymask) return -1;
  if (col<0) col=0; else if (col>=PS_GRID_COLC) col=PS_GRID_COLC-1;
  if (row<0) row=0; else if (row>=PS_GRID_ROWC) row=PS_GRID_ROWC-1;
  
  #define GO(dx,dy) { *dstcol=col+dx; *dstrow=row+dy; return 0; }
  
  if (phymask&(1<<grid->cellv[row*PS_GRID_COLC+col].physics)) GO(0,0)
  
  int east=((col>0)&&(phymask&(1<<grid->cellv[row*PS_GRID_COLC+col-1].physics)));
  int west=((col<PS_GRID_COLC-1)&&(phymask&(1<<grid->cellv[row*PS_GRID_COLC+col+1].physics)));
  int north=((row>0)&&(phymask&(1<<grid->cellv[(row-1)*PS_GRID_COLC+col].physics)));
  int south=((row<PS_GRID_ROWC-1)&&(phymask&(1<<grid->cellv[(row+1)*PS_GRID_COLC+col].physics)));
  int absxpref=(xpref<0)?-xpref:xpref;
  int absypref=(ypref<0)?-ypref:ypref;
  
  /* Do we have a strong preference for one direction? ie, preferring one axis over another */
  if (absxpref>absypref) {
    if (west&&(xpref<0)) GO(-1,0)
    if (east&&(xpref>0)) GO(1,0)
  } else if (absypref>absxpref) {
    if (north&&(ypref<0)) GO(0,-1)
    if (south&&(ypref>0)) GO(0,1)
  }
  
  /* Axes are equally preferred. Follow one of the two preferred directions if possible. */
  if ((xpref<0)&&west) GO(-1,0)
  if ((xpref>0)&&east) GO(1,0)
  if ((ypref<0)&&north) GO(0,-1)
  if ((ypref>0)&&south) GO(0,1)
  
  /* Can't get a preferred direction. Take whatever we can get. */
  if (west) GO(-1,0)
  if (east) GO(1,0)
  if (north) GO(0,-1)
  if (south) GO(0,1)
  
  #undef GO
  
  return -1;
}

int ps_grid_get_nearest_neighbor_matching_physics(
  int *dstcol,int *dstrow,const struct ps_grid *grid,int col,int row,uint16_t phymask
) {
  if (!dstcol||!dstrow||!grid||!phymask) return -1;
  if (col<0) col=0; else if (col>=PS_GRID_COLC) col=PS_GRID_COLC-1;
  if (row<0) row=0; else if (row>=PS_GRID_ROWC) row=PS_GRID_ROWC-1;
  
  if (phymask&(1<<grid->cellv[row*PS_GRID_COLC+col].physics)) {
    *dstcol=col;
    *dstrow=row;
    return 0;
  }
  
  int distance=1;
  for (;;distance++) {
    int result=ps_grid_get_nearest_neighbor_matching_physics_1(dstcol,dstrow,grid,col,row,phymask,distance);
    if (result<0) return -1;
    if (result>0) return 0;
  }
}
