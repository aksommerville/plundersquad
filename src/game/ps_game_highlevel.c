/* ps_game_highlevel.c
 * Trying to separate large-scale world events from the inner plumbing.
 */

#include "ps.h"
#include "ps_game.h"
#include "ps_sprite.h"
#include "ps_path.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "res/ps_resmgr.h"
#include "util/ps_geometry.h"

#define PS_BLOODHOUND_SPRDEF_ID 21

/* Summon bloodhound.
 */

static int ps_game_select_bloodhound_position(int *x,int *y,const struct ps_grid *grid) {

  // Anything on the edge, not a corner, and vacant is a candidate.
  int candidatec=0,i;
  for (i=1;i<PS_GRID_COLC-1;i++) {
    if (grid->cellv[i].physics==PS_BLUEPRINT_CELL_VACANT) candidatec++;
    if (grid->cellv[(PS_GRID_ROWC-1)*PS_GRID_COLC+i].physics==PS_BLUEPRINT_CELL_VACANT) candidatec++;
  }
  for (i=1;i<PS_GRID_ROWC-1;i++) {
    if (grid->cellv[i*PS_GRID_COLC].physics==PS_BLUEPRINT_CELL_VACANT) candidatec++;
    if (grid->cellv[i*PS_GRID_COLC+PS_GRID_COLC-1].physics==PS_BLUEPRINT_CELL_VACANT) candidatec++;
  }
  if (candidatec<1) return -1;

  int selection=rand()%candidatec;
  for (i=1;i<PS_GRID_COLC-1;i++) {
    if (grid->cellv[i].physics==PS_BLUEPRINT_CELL_VACANT) {
      if (!selection--) {
        *x=i;
        *y=-1;
        return 0;
      }
    }
    if (grid->cellv[(PS_GRID_ROWC-1)*PS_GRID_COLC+i].physics==PS_BLUEPRINT_CELL_VACANT) {
      if (!selection--) {
        *x=i;
        *y=PS_GRID_ROWC;
        return 0;
      }
    }
  }
  for (i=1;i<PS_GRID_ROWC-1;i++) {
    if (grid->cellv[i*PS_GRID_COLC].physics==PS_BLUEPRINT_CELL_VACANT) {
      if (!selection--) {
        *x=-1;
        *y=i;
        return 0;
      }
    }
    if (grid->cellv[i*PS_GRID_COLC+PS_GRID_COLC-1].physics==PS_BLUEPRINT_CELL_VACANT) {
      if (!selection--) {
        *x=PS_GRID_COLC;
        *y=i;
        return 0;
      }
    }
  }
  
  return -1;
}

int ps_game_summon_bloodhound(struct ps_game *game) {
  if (!game) return -1;
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_BLOODHOUND_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found (bloodhound)",PS_BLOODHOUND_SPRDEF_ID);
    return -1;
  }

  int x,y;
  if (ps_game_select_bloodhound_position(&x,&y,game->grid)<0) return -1;
  struct ps_sprite *spr=ps_sprdef_instantiate(game,sprdef,0,0,x*PS_TILESIZE+(PS_TILESIZE>>1),y*PS_TILESIZE+(PS_TILESIZE>>1));
  if (!spr) return -1;
  
  return 0;
}

/* Return a cardinal direction to the nearest treasure.
 */

static int ps_grid_has_uncollected_treasure(const struct ps_game *game,const struct ps_grid *grid) {
  const struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
      if (poi->argv[0]<0) continue;
      if (poi->argv[0]>=PS_TREASURE_LIMIT) continue;
      if (!game->treasurev[poi->argv[0]]) return 1;
    }
  }
  return 0;
}
 
int ps_game_get_direction_to_nearest_treasure(const struct ps_game *game) {
  if (!game) return -1;

  struct ps_treasure {
    int x,y;
    int distance;
    int direction;
  } treasurev[PS_TREASURE_LIMIT];
  int treasurec=0;

  /* Locate all uncollected treasures. */
  int screenc=game->scenario->w*game->scenario->h;
  const struct ps_screen *screen=game->scenario->screenv;
  for (;screenc-->0;screen++) {
    if (ps_grid_has_uncollected_treasure(game,screen->grid)) {
      struct ps_treasure *treasure=treasurev+treasurec++;
      treasure->x=screen->x;
      treasure->y=screen->y;
      if (ps_game_measure_distance_between_screens(
        &treasure->distance,&treasure->direction,
        game,treasure->x,treasure->y,game->gridx,game->gridy
      )<0) return -1;
      if (treasurec>=PS_TREASURE_LIMIT) break;
    }
  }
  if (treasurec<1) return -1;

  int best_direction=-1,best_distance=INT_MAX,i;
  for (i=0;i<treasurec;i++) {
    if (treasurev[i].distance<best_distance) {
      best_distance=treasurev[i].distance;
      best_direction=treasurev[i].direction;
    }
  }

  return best_direction;
}

/* Measure distance between screens.
 */
 
static int ps_game_measure_distance_between_screens_1(int *distance,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy,struct ps_path *path) {
  if ((srcx<0)||(srcx>=game->scenario->w)) return -1;
  if ((srcy<0)||(srcy>=game->scenario->h)) return -1;
  if ((dstx==srcx)&&(dsty==srcy)) return 0;
  if (ps_path_has(path,srcx,srcy)) return -1; // Been here.
  if (ps_path_add(path,srcx,srcy)<0) return -1; // Abort.

  (*distance)++;
  int distance0=(*distance);
  int screenc0=path->c,err,eastd=INT_MAX,westd=INT_MAX,northd=INT_MAX,southd=INT_MAX;
  const struct ps_screen *screen=game->scenario->screenv+srcy*game->scenario->w+srcx;

  if (screen->doorw==PS_DOOR_OPEN) {
    path->c=screenc0;
    *distance=distance0;
    if (ps_game_measure_distance_between_screens_1(distance,game,dstx,dsty,srcx-1,srcy,path)>=0) westd=*distance;
  }

  if (screen->doore==PS_DOOR_OPEN) {
    path->c=screenc0;
    *distance=distance0;
    if (ps_game_measure_distance_between_screens_1(distance,game,dstx,dsty,srcx+1,srcy,path)>=0) eastd=*distance;
  }

  if (screen->doorn==PS_DOOR_OPEN) {
    path->c=screenc0;
    *distance=distance0;
    if (ps_game_measure_distance_between_screens_1(distance,game,dstx,dsty,srcx,srcy-1,path)>=0) northd=*distance;
  }

  if (screen->doors==PS_DOOR_OPEN) {
    path->c=screenc0;
    *distance=distance0;
    if (ps_game_measure_distance_between_screens_1(distance,game,dstx,dsty,srcx,srcy+1,path)>=0) southd=*distance;
  }

  if ((westd<=eastd)&&(westd<=northd)&&(westd<=southd)) { *distance=westd; return PS_DIRECTION_WEST; }
  if ((eastd<=northd)&&(eastd<=southd)) { *distance=eastd; return PS_DIRECTION_EAST; }
  if (northd<=southd) { *distance=northd; return PS_DIRECTION_NORTH; }
  if (southd<INT_MAX) { *distance=southd; return PS_DIRECTION_SOUTH; }

  return -1;
}
 
int ps_game_measure_distance_between_screens(int *distance,int *direction,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy) {
  if (!distance||!direction||!game) return -1;
  if ((dstx==srcx)&&(dsty==srcy)) { *distance=*direction=0; return 0; }
  if ((dstx<0)||(dstx>=game->scenario->w)) return -1;
  if ((dsty<0)||(dsty>=game->scenario->h)) return -1;
  struct ps_path path={0};
  *distance=0;
  int err=ps_game_measure_distance_between_screens_1(distance,game,dstx,dsty,srcx,srcy,&path);
  ps_path_cleanup(&path);
  if (err<=0) return -1;
  *direction=err;
  return 0;
}

/* Similar to the above, but return a fully-formed path between screens.
 * Since the space of all screens is pretty small and paths between them are pretty limited, we go big and get the most correct answer.
 */
 
static int ps_game_compose_world_path_1(struct ps_path *path,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy) {
  if ((srcx<0)||(srcx>=game->scenario->w)) return -1;
  if ((srcy<0)||(srcy>=game->scenario->h)) return -1;
  if (ps_path_has(path,srcx,srcy)) return -1;
  if (ps_path_add(path,srcx,srcy)<0) return -1;
  if ((srcx==dstx)&&(srcy==dsty)) return 0;

  int c0=path->c;
  int scorew=INT_MAX,scoree=INT_MAX,scoren=INT_MAX,scores=INT_MAX;

  path->c=c0;
  if (ps_game_compose_world_path_1(path,game,dstx,dsty,srcx-1,srcy)>=0) scorew=path->c;

  path->c=c0;
  if (ps_game_compose_world_path_1(path,game,dstx,dsty,srcx+1,srcy)>=0) scoree=path->c;

  path->c=c0;
  if (ps_game_compose_world_path_1(path,game,dstx,dsty,srcx,srcy-1)>=0) scoren=path->c;

  path->c=c0;
  if (ps_game_compose_world_path_1(path,game,dstx,dsty,srcx,srcy+1)>=0) scores=path->c;

  path->c=c0;
  if ((scorew<scoree)&&(scorew<scoren)&&(scorew<scores)) {
    return ps_game_compose_world_path_1(path,game,dstx,dsty,srcx-1,srcy);
  } else if ((scoree<scoren)&&(scoree<scores)) {
    return ps_game_compose_world_path_1(path,game,dstx,dsty,srcx+1,srcy);
  } else if (scoren<scores) {
    return ps_game_compose_world_path_1(path,game,dstx,dsty,srcx,srcy-1);
  } else if (scores<INT_MAX) {
    return ps_game_compose_world_path_1(path,game,dstx,dsty,srcx,srcy+1);
  }
  return -1;
}
 
int ps_game_compose_world_path(struct ps_path *path,const struct ps_game *game,int dstx,int dsty,int srcx,int srcy) {
  if (!path||!game) return -1;
  if ((dstx<0)||(dstx>=game->scenario->w)) return -1;
  if ((dsty<0)||(dsty>=game->scenario->h)) return -1;
  path->c=0;
  return ps_game_compose_world_path_1(path,game,dstx,dsty,srcx,srcy);
}

/* Compose path in grid.
 * We do not use the same algorithm as the screen path generator.
 * The set of possible paths across a grid could be astronomically large, so we take an optimistic approach.
 * From each cell, examine the four possible moves in order of likelihood, and accept the first positive response.
 * We don't necessarily get the best path:
 *     +----------> (dst) <----+
 * === | ===================== | ===
 *     +--------------- (src)--+
 *          |                 |
 *      our result          ideal
 */
 
static int ps_game_compose_grid_path_1(struct ps_path *path,const struct ps_grid *grid,int dstx,int dsty,int srcx,int srcy,uint16_t impassable) {
  if (ps_path_has(path,srcx,srcy)) return -1;
  if ((srcx<-PS_GRID_COLC)||(srcx>PS_GRID_COLC<<1)) return -1;
  if ((srcy<-PS_GRID_ROWC)||(srcy>PS_GRID_ROWC<<1)) return -1;

  if (impassable) {
    int effectivex=(srcx<0)?0:(srcx>=PS_SCREENW)?(PS_SCREENW-1):srcx;
    int effectivey=(srcy<0)?0:(srcy>=PS_SCREENH)?(PS_SCREENH-1):srcy;
    if ((1<<grid->cellv[effectivey*PS_GRID_COLC+effectivex].physics)&impassable) return -1;
  }
  
  if (ps_path_add(path,srcx,srcy)<0) return -1;
  if ((srcx==dstx)&&(srcy==dsty)) return 0;
  int c0=path->c;

  /* Select the directions we can move from here, ordered by likeliest match. */
  int dirv[4];
  int dirc=0;
  int dx=dstx-srcx; int adx=(dx<0)?-dx:dx;
  int dy=dsty-srcy; int ady=(dy<0)?-dy:dy;
  if (adx>ady) { // horz first
    if (dx<0) {
      if (srcx>0) dirv[dirc++]=PS_DIRECTION_WEST;
    } else {
      if (srcx<PS_GRID_COLC-1) dirv[dirc++]=PS_DIRECTION_EAST;
    }
    if (dy<0) {
      if (srcy<PS_GRID_ROWC-1) dirv[dirc++]=PS_DIRECTION_SOUTH;
      if (srcy>0) dirv[dirc++]=PS_DIRECTION_NORTH;
    } else {
      if (srcy>0) dirv[dirc++]=PS_DIRECTION_NORTH;
      if (srcy<PS_GRID_ROWC-1) dirv[dirc++]=PS_DIRECTION_SOUTH;
    }
    if (dx<0) {
      if (srcx<PS_GRID_COLC-1) dirv[dirc++]=PS_DIRECTION_EAST;
    } else {
      if (srcx>0) dirv[dirc++]=PS_DIRECTION_WEST;
    }
  } else { // vert first
    if (dy<0) {
      if (srcy>0) dirv[dirc++]=PS_DIRECTION_NORTH;
    } else {
      if (srcy<PS_GRID_ROWC-1) dirv[dirc++]=PS_DIRECTION_SOUTH;
    }
    if (dx<0) {
      if (srcx>0) dirv[dirc++]=PS_DIRECTION_WEST;
      if (srcx<PS_GRID_COLC-1) dirv[dirc++]=PS_DIRECTION_EAST;
    } else {
      if (srcx<PS_GRID_COLC-1) dirv[dirc++]=PS_DIRECTION_EAST;
      if (srcx>0) dirv[dirc++]=PS_DIRECTION_WEST;
    }
    if (dy<0) {
      if (srcy<PS_GRID_ROWC-1) dirv[dirc++]=PS_DIRECTION_SOUTH;
    } else {
      if (srcy>0) dirv[dirc++]=PS_DIRECTION_NORTH;
    }
  }

  /* Consider each of those directions in turn.
   * If we get a match, return it immediately.
   */
  int i; for (i=0;i<dirc;i++) {
    struct ps_vector d=ps_vector_from_direction(dirv[i]);
    path->c=c0;
    int err=ps_game_compose_grid_path_1(path,grid,dstx,dsty,srcx+d.dx,srcy+d.dy,impassable);
    if (err>=0) return 0;
  }
  
  return -1;
}
 
int ps_game_compose_grid_path(struct ps_path *path,const struct ps_grid *grid,int dstx,int dsty,int srcx,int srcy,uint16_t impassable) {
  if (!path||!grid) return -1;
  if ((dstx<-PS_SCREENW)||(dstx>PS_SCREENW<<1)) return -1;
  if ((dsty<-PS_SCREENH)||(dsty>PS_SCREENH<<1)) return -1;
  path->c=0;
  int err=ps_game_compose_grid_path_1(path,grid,dstx,dsty,srcx,srcy,impassable);
  return err;
}
