/* ps_game_highlevel.c
 * Trying to separate large-scale world events from the inner plumbing.
 */

#include "ps.h"
#include "ps_game.h"
#include "ps_sprite.h"
#include "ps_path.h"
#include "ps_sound_effects.h"
#include "ps_physics.h"
#include "ps_switchboard.h"
#include "ps_summoner.h"
#include "ps_player.h"
#include "ps_stats.h"
#include "ps_plrdef.h"
#include "ps_gamelog.h"
#include "ps_score_store.h"
#include "sprites/ps_sprite_hero.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_region.h"
#include "res/ps_resmgr.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_map.h"
#include "util/ps_geometry.h"
#include "util/ps_text.h"
#include "util/ps_enums.h"
#include <math.h>

#define PS_BLOODHOUND_SPRDEF_ID 21
#define PS_TREASURE_SPRDEF_ID 12

void ps_game_force_hero_to_legal_position(struct ps_game *game,struct ps_sprite *hero);
int ps_game_spawn_sprites(struct ps_game *game);
int ps_game_register_switches(struct ps_game *game);
int ps_game_npgc_pop(struct ps_game *game);
int ps_game_kill_nonhero_sprites(struct ps_game *game);
int ps_game_setup_deathgate(struct ps_game *game);
int ps_game_check_status_report(struct ps_game *game);

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
 
static int ps_game_compose_grid_path_1(struct ps_path *path,const struct ps_grid *grid,int dstx,int dsty,int srcx,int srcy,uint16_t impassable,struct ps_path *blacklist) {
  if (ps_path_has(path,srcx,srcy)) return -1;
  if (ps_path_has(blacklist,srcx,srcy)) return -1;
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
    int err=ps_game_compose_grid_path_1(path,grid,dstx,dsty,srcx+d.dx,srcy+d.dy,impassable,blacklist);
    if (err>=0) return 0;
  }

  /* Can't get there from here; ensure that we don't re-enter this cell. */
  if (ps_path_add(blacklist,srcx,srcy)<0) return -1;
  
  return -1;
}
 
int ps_game_compose_grid_path(struct ps_path *path,const struct ps_grid *grid,int dstx,int dsty,int srcx,int srcy,uint16_t impassable) {
  if (!path||!grid) return -1;
  if ((dstx<-PS_SCREENW)||(dstx>PS_SCREENW<<1)) return -1;
  if ((dsty<-PS_SCREENH)||(dsty>PS_SCREENH<<1)) return -1;
  struct ps_path blacklist={0};
  path->c=0;
  int err=ps_game_compose_grid_path_1(path,grid,dstx,dsty,srcx,srcy,impassable,&blacklist);
  ps_path_cleanup(&blacklist);
  return err;
}

/* Locate contiguous physical rect.
 */

static int ps_game_row_physics_match(const struct ps_grid_cell *cell,int y,int x,int w,uint8_t physics) {
  cell+=y*PS_GRID_COLC;
  cell+=x;
  for (;w-->0;cell++) {
    if (cell->physics!=physics) return 0;
  }
  return 1;
}
 
int ps_game_get_contiguous_physical_rect_in_grid(int *dstx,int *dsty,int *dstw,int *dsth,const struct ps_grid *grid,int x,int y) {
  if (!dstx||!dsty||!dstw||!dsth||!grid) return -1;
  if ((x<0)||(x>=PS_GRID_COLC)) return -1;
  if ((y<0)||(y>=PS_GRID_ROWC)) return -1;

  *dstx=x;
  *dsty=y;
  *dstw=1;
  *dsth=1;
  uint8_t physics=grid->cellv[y*PS_GRID_COLC+x].physics;

  /* Expand along the row. */
  while ((*dstx>0)&&(grid->cellv[(*dsty)*PS_GRID_COLC+(*dstx)-1].physics==physics)) { (*dstx)--; (*dstw)++; }
  while ((*dstx+(*dstw)<PS_GRID_COLC)&&(grid->cellv[(*dsty)*PS_GRID_COLC+(*dstx)+(*dstw)].physics==physics)) (*dstw)++;

  /* Consume additional rows if they match entirely. */
  while ((*dsty>0)&&ps_game_row_physics_match(grid->cellv,(*dsty)-1,*dstx,*dstw,physics)) { (*dsty)--; (*dsth)++; }
  while ((*dsty+(*dsth)<PS_GRID_ROWC)&&ps_game_row_physics_match(grid->cellv,(*dsty)+(*dsth),*dstx,*dstw,physics)) (*dsth)++;
  
  return 0;
}

/* Standard groups as mask.
 */
 
uint32_t ps_game_get_group_mask_for_sprite(const struct ps_game *game,const struct ps_sprite *spr) {
  if (!game||!spr) return 0;
  uint32_t mask=0,bit=1,i=0;
  for (;i<PS_SPRGRP_COUNT;i++,bit<<=1) {
    if (ps_sprgrp_has_sprite(game->grpv+i,spr)) mask|=bit;
  }
  return mask;
}

int ps_game_set_group_mask_for_sprite(struct ps_game *game,struct ps_sprite *spr,uint32_t grpmask) {
  if (!game||!spr) return -1;
  if (ps_sprite_ref(spr)<0) return -1; // This operation could kill the sprite inadvertently, so retain throughout.
  uint32_t bit=1,i=0;
  for (;i<PS_SPRGRP_COUNT;i++,bit<<=1) {
    if (grpmask&bit) {
      if (ps_sprgrp_add_sprite(game->grpv+i,spr)<0) {
        ps_sprite_del(spr);
        return -1;
      }
    } else {
      if (ps_sprgrp_remove_sprite(game->grpv+i,spr)<0) {
        ps_sprite_del(spr);
        return -1;
      }
    }
  }
  ps_sprite_del(spr);
  return 0;
}

/* Hero indicator management.
 */
 
int ps_game_add_indicator_for_hero(struct ps_game *game,struct ps_sprite *hero) {
  if (!game||!hero) return -1;
  if (hero->type!=&ps_sprtype_hero) return -1;

  struct ps_sprite *spr=ps_sprite_new(&ps_sprtype_heroindicator);
  if (!spr) return -1;
  if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_KEEPALIVE,spr)<0) {
    ps_sprite_del(spr);
    return -1;
  }
  ps_sprite_del(spr);

  if (ps_game_set_group_mask_for_sprite(game,spr,
    (1<<PS_SPRGRP_VISIBLE)|
    (1<<PS_SPRGRP_UPDATE)|
    (1<<PS_SPRGRP_HERO)|
  0)<0) return -1;

  if (ps_sprite_heroindicator_set_hero(spr,hero)<0) {
    ps_sprite_kill(spr);
    return -1;
  }

  if (ps_sprite_update(spr,game)<0) return -1;
  
  return 0;
}

int ps_game_remove_indicator_for_hero(struct ps_game *game,struct ps_sprite *hero) {
  if (!game||!hero) return -1;
  int i=game->grpv[PS_SPRGRP_HERO].sprc;
  while (i-->0) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if (spr->type!=&ps_sprtype_heroindicator) continue;
    if (ps_sprite_heroindicator_get_hero(spr)==hero) {
      if (ps_sprite_kill_later(spr,game)<0) return -1;
    }
  }
  return 0;
}

/* Events on monster death.
 */
 
int ps_game_decorate_monster_death(struct ps_game *game,int x,int y) {
  PS_SFX_MONSTER_DEAD
  if (ps_game_create_fireworks(game,x,y)<0) return -1;
  if (ps_game_create_prize(game,x,y)<0) return -1;
  if (ps_game_check_deathgate(game)<0) return -1;
  return 0;
}

/* Advance to finish (DEBUG).
 */
 
int ps_game_advance_to_finish(struct ps_game *game) {
  if (!game) return -1;

  int i; for (i=0;i<game->treasurec;i++) game->treasurev[i]=1;
  int final_treasureid=game->treasurec-1;
  game->treasurev[final_treasureid]=0;

  struct ps_grid *dstgrid=0;
  int dstx,dsty;
  struct ps_screen *screen=game->scenario->screenv;
  for (i=game->scenario->w*game->scenario->h;i-->0;screen++) {
    if (!screen->grid) continue;
    const struct ps_blueprint_poi *poi=screen->grid->poiv;
    int j=screen->grid->poic; for (;j-->0;poi++) {
      if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
        if (poi->argv[0]==final_treasureid) {
          dstgrid=screen->grid;
          dstx=screen->x;
          dsty=screen->y;
          break;
        }
      }
    }
  }

  if (dstgrid) {
    if (ps_game_change_screen(game,dstx,dsty,PS_CHANGE_SCREEN_MODE_WARP)<0) return -1;
  }
  
  return 0;
}

/* Dump award decision matrix.
 */

static void ps_dump_award_decision_matrix(int *decisionv) {
  char buf[256];
  int bufc,ai,pi;
  const int colw=5;
  
  ps_log(GAME,DEBUG,"===== Award decision matrix =====");

  bufc=ps_strcpy(buf,sizeof(buf),"PLAYER",6);
  for (ai=0;ai<PS_AWARD_COUNT;ai++) {
    if (bufc<sizeof(buf)) buf[bufc]=' '; bufc++;
    const char *name=ps_award_repr(ai);
    int namec=0; if (name) while (name[namec]) namec++;
    if (namec<colw) bufc+=ps_strcpy(buf+bufc,sizeof(buf)-bufc,"                ",colw-namec);
    else if (namec>colw) namec=colw;
    bufc+=ps_strcpy(buf+bufc,sizeof(buf)-bufc,name,namec);
  }
  ps_log(GAME,DEBUG,"%.*s",bufc,buf);

  for (pi=0;pi<PS_PLAYER_LIMIT;pi++) {
    bufc=ps_strcpy(buf,sizeof(buf),"     ",5);
    buf[bufc++]='1'+pi;
    for (ai=0;ai<PS_AWARD_COUNT;ai++) {
      bufc+=snprintf(buf+bufc,sizeof(buf)-bufc,"%6d",*decisionv++);
    }
    ps_log(GAME,DEBUG,"%.*s",bufc,buf);
  }
}

static void ps_dump_award_weights(double *weights) {
  ps_log(GAME,DEBUG,"===== Award criteria =====");
  int i=0; for (;i<PS_AWARD_COUNT;i++) {
    ps_log(GAME,DEBUG,"%3d: %.6f %s",i,weights[i],ps_award_describe(i));
  }
}

/* Select award for one-player game.
 */

static int ps_game_assign_award_solo(struct ps_game *game) {
  const struct ps_stats_player *pstats=game->stats->playerv;
  struct ps_player *player=game->playerv[0];
  double weightv[PS_AWARD_COUNT]={0};

  /* Score each award by some reasonable standard, in 0..1.
   * Some may theoretically exceed 1.0, so we clamp them.
   */
  weightv[PS_AWARD_NONE]=0.0;
  weightv[PS_AWARD_PARTICIPANT]=0.1;
  weightv[PS_AWARD_WALKER]=(double)pstats->stepc/(double)game->stats->playtime;
  weightv[PS_AWARD_KILLER]=(pstats->killc_monster*600.0)/game->stats->playtime; // kills per ten seconds
  if (weightv[PS_AWARD_KILLER]>1.0) weightv[PS_AWARD_KILLER]=1.0;
  weightv[PS_AWARD_TRAITOR]=0.0; // Not relevant
  weightv[PS_AWARD_DEATH]=(pstats->deathc*600.0)/game->stats->playtime; // deaths per ten seconds
  if (weightv[PS_AWARD_DEATH]>1.0) weightv[PS_AWARD_DEATH]=1.0;
  weightv[PS_AWARD_ACTUATOR]=(pstats->switchc*60.0)/game->stats->playtime; // This will never win, which is fine.
  if (weightv[PS_AWARD_ACTUATOR]>1.0) weightv[PS_AWARD_ACTUATOR]=1.0;
  weightv[PS_AWARD_LIVELY]=(double)pstats->framec_alive/(double)game->stats->playtime;
  weightv[PS_AWARD_GHOST]=1.0-weightv[PS_AWARD_LIVELY];
  weightv[PS_AWARD_TREASURE]=0.0; // Not relevant
  weightv[PS_AWARD_LAZY]=1.0-weightv[PS_AWARD_WALKER];
  weightv[PS_AWARD_GENEROUS]=0.0; // Not relevant
  weightv[PS_AWARD_HARD_TARGET]=1.0-weightv[PS_AWARD_DEATH];

  /* Apply arbitrary weights to each award based on how important I feel each is.
   */
  weightv[PS_AWARD_WALKER]*=0.4;
  weightv[PS_AWARD_LIVELY]*=0.4;
  weightv[PS_AWARD_GHOST]*=0.4;
  weightv[PS_AWARD_LAZY]*=0.4;

  ps_dump_award_weights(weightv); // XXX TEMP

  /* Select the highest.
   */
  double max=0.0;
  int i=PS_AWARD_COUNT; while (i-->0) {
    if (weightv[i]>max) {
      player->award=i;
      max=weightv[i];
    }
  }
  return 0;
}

/* Assign an award to each player.
 */

static const int ps_award_preferencev[PS_AWARD_COUNT]={
  PS_AWARD_TRAITOR,
  PS_AWARD_KILLER,
  PS_AWARD_HARD_TARGET,
  PS_AWARD_TREASURE,
  PS_AWARD_DEATH,
  PS_AWARD_LIVELY,
  PS_AWARD_LAZY,
  PS_AWARD_PACIFIST,
  PS_AWARD_GENEROUS,
  PS_AWARD_GHOST,
  PS_AWARD_ACTUATOR,
  PS_AWARD_WALKER,
  PS_AWARD_PARTICIPANT,
  PS_AWARD_NONE,
};
 
int ps_game_assign_awards(struct ps_game *game) {
  if (!game) return -1;
  if (game->playerc<1) return 0;
  const struct ps_stats_player *pstats;

  /* The logic for a one-player game is completely different.
   */
  if (game->playerc==1) return ps_game_assign_award_solo(game);

  /* Start with a matrix of all awards and all players.
   * Higher values mean that assignment is more appropriate.
   * They all start at one.
   */
  int decisionc=PS_AWARD_COUNT*PS_PLAYER_LIMIT;
  int decisionv[PS_AWARD_COUNT*PS_PLAYER_LIMIT];
  int i; for (i=decisionc;i-->0;) decisionv[i]=1;

  /* Eliminate the NONE award, and eliminate all nonexistent players.
   */
  for (i=0;i<game->playerc;i++) decisionv[i*PS_AWARD_COUNT+PS_AWARD_NONE]=0;
  memset(decisionv+PS_AWARD_COUNT*game->playerc,0,sizeof(int)*PS_AWARD_COUNT*(PS_PLAYER_LIMIT-game->playerc));

  /* Any player with the IMMORTAL skill is ineligible for LIVELY and HARD_TARGET.
   * Otherwise he would get them every time, just for having chosen that hero.
   * Likewise, anyone lacking COMBAT is ineligible for PACIFIST.
   */
  for (i=0;i<game->playerc;i++) {
    struct ps_player *player=game->playerv[i];
    uint16_t skills=player->plrdef->skills;
    if (skills&PS_SKILL_IMMORTAL) {
      decisionv[PS_AWARD_COUNT*i+PS_AWARD_LIVELY]=0;
      decisionv[PS_AWARD_COUNT*i+PS_AWARD_HARD_TARGET]=0;
    }
    if (!(skills&PS_SKILL_COMBAT)) {
      decisionv[PS_AWARD_COUNT*i+PS_AWARD_PACIFIST]=0;
    }
  }

  /* Collect aggregates from player stats: Highest death and kill counts.
   */
  int maxdeathc=0,maxkillc=0;
  for (i=0,pstats=game->stats->playerv;i<game->playerc;i++,pstats++) {
    if (pstats->deathc>maxdeathc) maxdeathc=pstats->deathc;
    int killc=pstats->killc_monster+pstats->killc_hero;
    if (killc>maxkillc) maxkillc=killc;
  }

  /* Fill each nonzero slot with some absolute numbers to get things started.
   */
  for (i=0,pstats=game->stats->playerv;i<game->playerc;i++,pstats++) {
    #define SETIF(tag,value) if (decisionv[PS_AWARD_COUNT*i+PS_AWARD_##tag]) decisionv[PS_AWARD_COUNT*i+PS_AWARD_##tag]=(value);
    SETIF(WALKER,pstats->stepc)
    SETIF(KILLER,pstats->killc_monster)
    SETIF(TRAITOR,pstats->killc_hero)
    SETIF(DEATH,pstats->deathc)
    SETIF(ACTUATOR,pstats->switchc)
    SETIF(LIVELY,pstats->framec_alive)
    SETIF(GHOST,game->stats->playtime-pstats->framec_alive)
    SETIF(TREASURE,pstats->treasurec)
    SETIF(LAZY,game->stats->playtime-pstats->stepc)
    SETIF(PACIFIST,maxkillc-pstats->killc_monster-pstats->killc_hero)
    SETIF(GENEROUS,game->treasurec-pstats->treasurec)
    SETIF(HARD_TARGET,maxdeathc-pstats->deathc)
    #undef SETIF
  }

  /* Reduce each column; any value which is not the highest in its column becomes zero.
   * Reasoning: You can only get an award if you are the best, or tied for best.
   * Also, accept all-way ties only for the PARTICIPANT award.
   */
  for (i=0;i<PS_AWARD_COUNT;i++) {
    int max=0,maxc=0;
    int pi=0; for (;pi<game->playerc;pi++) {
      int p=pi*PS_AWARD_COUNT+i;
      if (decisionv[p]>max) {
        max=decisionv[p];
        maxc=1;
      } else if (decisionv[p]==max) maxc++;
    }
    if ((maxc==game->playerc)&&(i!=PS_AWARD_PARTICIPANT)) {
      for (pi=0;pi<game->playerc;pi++) {
        int p=pi*PS_AWARD_COUNT+i;
        decisionv[p]=0;
      }
    } else {
      for (pi=0;pi<game->playerc;pi++) {
        int p=pi*PS_AWARD_COUNT+i;
        if (decisionv[p]!=max) decisionv[p]=0;
      }
    }
  }

  ps_dump_award_decision_matrix(decisionv);//XXX TEMP

  /* For each player, select any nonzero award, in a preset order of preference.
   * Every player still has a one in the PARTICIPATION slot, so they'll definitely get something.
   * TODO Is it worth weighting and comparing the various awards, to find the most salient?
   */
  for (i=0;i<game->playerc;i++) {
    struct ps_player *player=game->playerv[i];
    int *awards=decisionv+PS_AWARD_COUNT*i;
    int api=0; for (;api<PS_AWARD_COUNT;api++) {
      if (awards[ps_award_preferencev[api]]) {
        player->award=ps_award_preferencev[api];
        ps_log(GAME,DEBUG,"Player %d: %s",i+1,ps_award_describe(player->award));
        break;
      }
    }
  }

  return 0;
}

/* Highlight enabled players.
 */
 
int ps_game_highlight_enabled_players(struct ps_game *game) {
  if (!game) return -1;
  int i=game->grpv[PS_SPRGRP_HERO].sprc;
  while (i-->0) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if (spr->type!=&ps_sprtype_hero) continue;
    struct ps_sprite_hero *hero=(struct ps_sprite_hero*)spr;
    if (!hero->player) continue;
    uint16_t buttons=ps_get_player_buttons(hero->player->playerid);
    if (!(buttons&PS_PLRBTN_CD)) continue;
    if (ps_hero_add_state(spr,PS_HERO_STATE_HIGHLIGHT,game)<0) return -1;
  }
  return 0;
}

/* Find random cell with physics.
 */
 
int ps_game_find_random_cell_with_physics(int *col,int *row,const struct ps_game *game,uint16_t mask) {
  if (!col||!row||!game) return -1;
  if (!game->grid) return -1;
  if (!mask) return -1; // Impossible.

  /* Before doing anything expensive, try a few random stabs.
   */
  int repc=10;
  while (repc-->0) {
    int x=rand()%PS_GRID_COLC;
    int y=rand()%PS_GRID_ROWC;
    uint8_t physics=game->grid->cellv[y*PS_GRID_COLC+x].physics;
    if (mask&(1<<physics)) {
      *col=x;
      *row=y;
      return 0;
    }
  }

  /* Compose a list of all matching cells.
   */
  struct ps_cell_coord {
    uint8_t x,y;
  } coordv[PS_GRID_COLC*PS_GRID_ROWC];
  int coordc=0;
  const struct ps_grid_cell *cell=game->grid->cellv;
  int y=0; for (;y<PS_GRID_ROWC;y++) {
    int x=0; for (;x<PS_GRID_COLC;x++,cell++) {
      if (mask&(1<<cell->physics)) {
        coordv[coordc].x=x;
        coordv[coordc].y=y;
        coordc++;
      }
    }
  }

  /* Select one randomly.
   */
  if (coordc<1) return -1;
  int coordp=rand()%coordc;
  *col=coordv[coordp].x;
  *row=coordv[coordp].y;
  return 0;
}

/* Save to score store.
 */
 
int ps_game_save_to_score_store(struct ps_game *game) {
  if (!game) return -1;
  if (!game->score_store) return -1;
  if (!game->stats) return -1;

  /* Gather the per-player stats. */
  int monster_kills=0,hero_kills=0,hero_deaths=0,i;
  for (i=game->playerc;i-->0;) {
    monster_kills+=game->stats->playerv[i].killc_monster;
    hero_kills+=game->stats->playerv[i].killc_hero;
    hero_deaths+=game->stats->playerv[i].deathc;
  }

  /* hero_deaths shouldn't count hero_kills; it's the "heroes killed by other" */
  if ((hero_deaths-=hero_kills)<0) hero_deaths=0;

  /* Let score store take over. */
  if (ps_score_store_add_record(game->score_store,game)<0) {
    // Log the error but don't cascade it.
    ps_log(GAME,ERROR,"Failed to save to score store.");
    return 0;
  }
  
  return 0;
}

/* Connect or disconnect input devices.
 */
 
int ps_game_cb_device_connect(struct ps_input_device *device,void *userdata) {
  struct ps_game *game=userdata;
  if (game->finished) return 0;
  if (game->paused) return 0;
  if (game->playerc<1) return 0; // Not initialized
  if (!device) return 0; // Um, that's not OK.
  if (!device->map) return 0; // Can't assign unmapped devices, just ignore it.
  int playerid=device->map->plrid;
  if ((playerid>=1)&&(playerid<=PS_PLAYER_LIMIT)) {
    ps_log(GAME,DEBUG,"Connect device '%s', playerid %d.",device->name,playerid);
  } else {
    if (ps_input_assign_device_to_least_covered_player(device,game->playerc)<0) return -1;
  }
  return 0;
}

int ps_game_cb_device_disconnect(struct ps_input_device *device,void *userdata) {
  struct ps_game *game=userdata;
  return 0;
}

/* Create treasure chest for realsies.
 */
 
int ps_game_recreate_treasure_chest(struct ps_game *game,int x,int y) {
  if (!game||!game->grid) return 0;

  const int *argv=0;
  int argc=0;
  int treasureid=-1;
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  int i=game->grid->poic; for (;i-->0;poi++) {
    if (poi->type!=PS_BLUEPRINT_POI_TREASURE) continue;
    treasureid=poi->argv[0];
    argv=poi->argv;
    argc=3;
    break;
  }
  if (treasureid<0) return 0; // no treasure POI
  if (treasureid>=game->treasurec) return 0;
  if (game->treasurev[treasureid]) return 0;
  
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_TREASURE_SPRDEF_ID);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found",PS_TREASURE_SPRDEF_ID);
    return -1;
  }
  struct ps_sprite *sprite=ps_sprdef_instantiate(game,sprdef,argv,argc,x,y);
  if (!sprite) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",PS_TREASURE_SPRDEF_ID);
    return -1;
  }
  
  return 0;
}

/* Select a random vector for monster travel.
 */
 
int ps_game_select_random_travel_vector(
  double *dx,double *dy,const struct ps_game *game,double x,double y,double speed,uint16_t impassable
) {
  if (!dx||!dy||!game) return -1;
  if (speed<0.1) speed=0.1;
  if (x<1.0) x=1.0;
  else if (x>=PS_SCREENW-1.0) x=PS_SCREENW-1.0;
  if (y<1.0) y=1.0;
  else if (y>=PS_SCREENH-1.0) y=PS_SCREENH-1.0;
  int srccol=(int)x/PS_TILESIZE;
  int srcrow=(int)y/PS_TILESIZE;
  
  /* Begin some short distance out, say 4 tiles, and select a random vector.
   * If it collides with anything (even screen edge), cut the distance and try again.
   * Once we've reached the bottom, return anything.
   * Our contract is best-effort: It's totally fine if monsters ram into the wall from time to time.
   */
  double distance=PS_TILESIZE*4.0+2.0;
  while (distance>=PS_TILESIZE) {
    int tryc=5;
    while (tryc-->0) {
      double t=(rand()%6282)/1000.0;
      *dx=-sin(t);
      *dy=cos(t);
      double dstx=x+(*dx)*distance;
      double dsty=y+(*dy)*distance;
      if ((dstx>0.0)&&(dsty>0.0)&&(dstx<PS_SCREENW)&&(dsty<PS_SCREENH)) {
        int dstcol=(int)dstx/PS_TILESIZE;
        int dstrow=(int)dsty/PS_TILESIZE;
        if (!ps_grid_line_contains_physics(game->grid,srccol,srcrow,dstcol,dstrow,impassable)) {
          (*dx)*=speed;
          (*dy)*=speed;
          return 0;
        }
      }
    }
    distance-=PS_TILESIZE;
  }
  
  (*dx)*=speed;
  (*dy)*=speed;
  return 0;
}

/* React to grid changes.
 *  1. Don't panic -- Most changes will be managed sanely by the next physics pass.
 *  2. If a cell becomes SOLID or LATCH, identify sprites:
 *    2a. ...whose center is in that cell
 *    2b. ...who are in group SOLID
 *    2c. ...and whose impassable mask matches the cell.
 *  3. If any cardinal neighbor is VACANT, HEROONLY, or HEAL, move the sprite there.
 *  4. Otherwise if any cardinal neighbor is HOLE, move the sprite there (it will likely die on the next update).
 *  5. Otherwise, hurt the sprite and move it to the nearest passable cell.
 * This is necessary to prevent heroes from erroring across a wall when a door shuts on them (which did happen before).
 * We only look at the sprite's center because it's cheaper, and we can otherwise depend on physics to correct the situation.
 */
 
static int ps_game_adjust_sprite_for_grid_change(struct ps_game *game,struct ps_sprite *spr,int col,int row) {
  uint16_t friendly_physics=
    (1<<PS_BLUEPRINT_CELL_VACANT)|
    (1<<PS_BLUEPRINT_CELL_HEROONLY)|
    (1<<PS_BLUEPRINT_CELL_HEAL)|
  0;
  uint16_t possible_physics=
    friendly_physics|
    (1<<PS_BLUEPRINT_CELL_HAZARD)|
    (1<<PS_BLUEPRINT_CELL_HOLE)|
  0;
  int dstcol,dstrow;
  if (ps_grid_get_cardinal_neighbor_matching_physics(&dstcol,&dstrow,game->grid,col,row,friendly_physics)>=0) {
    spr->x=dstcol*PS_TILESIZE+(PS_TILESIZE>>1);
    spr->y=dstrow*PS_TILESIZE+(PS_TILESIZE>>1);
    return 0;
  }
  if (ps_grid_get_cardinal_neighbor_matching_physics(&dstcol,&dstrow,game->grid,col,row,1<<PS_BLUEPRINT_CELL_HOLE)>=0) {
    spr->x=dstcol*PS_TILESIZE+(PS_TILESIZE>>1);
    spr->y=dstrow*PS_TILESIZE+(PS_TILESIZE>>1);
    // Hero update would kill it in this case, but physics will see it first and toss it away. Kill manually.
    if (ps_sprite_receive_damage(game,spr,0)<0) return -1;
    return 0;
  }
  if (ps_grid_get_nearest_neighbor_matching_physics(&dstcol,&dstrow,game->grid,col,row,possible_physics)>=0) {
    spr->x=dstcol*PS_TILESIZE+(PS_TILESIZE>>1);
    spr->y=dstrow*PS_TILESIZE+(PS_TILESIZE>>1);
    if (ps_sprite_receive_damage(game,spr,0)<0) return -1;
    return 0;
  }
  if (ps_sprite_receive_damage(game,spr,0)<0) return -1;
  return 0;
}
 
static int ps_game_adjust_sprites_for_grid_change(struct ps_game *game,int col,int row,uint8_t physics) {
  uint16_t physicsmask=1<<physics;
  if (!physicsmask) return 0;
  double left=col*PS_TILESIZE;
  double top=row*PS_TILESIZE;
  double right=left+PS_TILESIZE;
  double bottom=top+PS_TILESIZE;
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_SOLID;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *spr=grp->sprv[i];
    if (!(spr->impassable&physicsmask)) continue;
    if (spr->x<left) continue;
    if (spr->x>=right) continue;
    if (spr->y<top) continue;
    if (spr->y>=bottom) continue;
    if (ps_game_adjust_sprite_for_grid_change(game,spr,col,row)<0) return -1;
  }
  return 0;
}
 
int ps_game_adjust_sprites_for_grid_changes(struct ps_game *game,const struct ps_path *changes) {
  if (!game||!changes) return 0;
  if (!game->grid) return 0;
  const struct ps_path_entry *entry=changes->v;
  int i=changes->c; for (;i-->0;entry++) {
    uint8_t physics=game->grid->cellv[entry->y*PS_GRID_COLC+entry->x].physics;
    if ((physics==PS_BLUEPRINT_CELL_SOLID)||(physics==PS_BLUEPRINT_CELL_LATCH)) {
      if (ps_game_adjust_sprites_for_grid_change(game,entry->x,entry->y,physics)<0) {
        return -1;
      }
    }
  }
  return 0;
}
