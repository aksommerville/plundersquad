#include "ps.h"
#include "ps_game.h"
#include "ps_summoner.h"
#include "ps_sprite.h"
#include "ps_path.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "util/ps_geometry.h"
#include "res/ps_resmgr.h"

/* New.
 */
 
struct ps_summoner *ps_summoner_new() {
  struct ps_summoner *summoner=calloc(1,sizeof(struct ps_summoner));
  if (!summoner) return 0;

  summoner->refc=1;

  return summoner;
}

/* Delete.
 */

static void ps_summoner_entry_cleanup(struct ps_summoner_entry *entry) {
  ps_sprgrp_clear(entry->alive);
  ps_sprgrp_del(entry->alive);
  if (entry->cells) {
    ps_path_cleanup(entry->cells);
    free(entry->cells);
  }
}

void ps_summoner_del(struct ps_summoner *summoner) {
  if (!summoner) return;
  if (summoner->refc-->1) return;

  if (summoner->entryv) {
    while (summoner->entryc-->0) {
      ps_summoner_entry_cleanup(summoner->entryv+summoner->entryc);
    }
    free(summoner->entryv);
  }

  free(summoner);
}

/* Retain.
 */

int ps_summoner_ref(struct ps_summoner *summoner) {
  if (!summoner) return -1;
  if (summoner->refc<1) return -1;
  if (summoner->refc==INT_MAX) return -1;
  summoner->refc++;
  return 0;
}

/* Set new delay for entry.
 */

static int ps_summoner_sprite_limit_for_volume(int volume) {
  int limit=volume/20+1;
  return limit;
}

static int ps_summoner_delay_for_volume(int volume) {
  int delay=(100-volume)*2+60;
  if (delay<60) return 60;
  if (delay>240) return 240;
  return delay;
}

static int ps_summoner_entry_reset_delay(struct ps_summoner_entry *entry) {
  entry->delay=ps_summoner_delay_for_volume(entry->volume);
  return 0;
}

/* Populate entry path via seed fill.
 */

static int ps_summoner_entry_setup_path_seed_fill(struct ps_summoner_entry *entry,int x,int y,const struct ps_grid *grid) {

  if (ps_path_has(entry->cells,x,y)) return 0;
  if (ps_path_add(entry->cells,x,y)<0) return -1;
  uint8_t physics=grid->cellv[y*PS_GRID_COLC+x].physics;

  struct ps_vector neighborv[4]={
    {x-1,y},{x+1,y},{x,y-1},{x,y+1},
  };
  int i=4; while (i-->0) {
    x=neighborv[i].dx;
    y=neighborv[i].dy;
    if ((x<0)||(y<0)||(x>=PS_GRID_COLC)||(y>=PS_GRID_ROWC)) continue;
    if (grid->cellv[y*PS_GRID_COLC+x].physics!=physics) continue;
    if (ps_summoner_entry_setup_path_seed_fill(entry,x,y,grid)<0) return -1;
  }
  return 0;
}

/* Populate entry path by walking over the edge and spreading out from there.
 */

static int ps_summoner_entry_setup_path_edge_1(const struct ps_grid *grid,int *x,int *y,int dx,int dy,struct ps_path *blacklist) {

  if ((dx<0)&&!*x) return 1;
  if ((dy<0)&&!*y) return 1;
  if ((dx>0)&&(*x==PS_GRID_COLC-1)) return 1;
  if ((dy>0)&&(*y==PS_GRID_ROWC-1)) return 1;

  int x0=*x,y0=*y;
  if (ps_path_has(blacklist,x0,y0)) return 0;
  if (ps_path_add(blacklist,x0,y0)) return -1;
  uint8_t physics=grid->cellv[y0*PS_GRID_COLC+x0].physics;
  
  struct ps_vector neighborv[4];
  neighborv[0]=ps_vector(x0+dx,y0+dy);
  if (dx) {
    neighborv[1]=ps_vector(x0,y0-1);
    neighborv[2]=ps_vector(x0,y0+1);
  } else {
    neighborv[1]=ps_vector(x0-1,y0);
    neighborv[2]=ps_vector(x0+1,y0);
  }
  neighborv[3]=ps_vector(x0-dx,y0-dy);
  int i=0; for (;i<4;i++) {
    *x=neighborv[i].dx;
    *y=neighborv[i].dy;
    if ((*x<0)||(*y<0)||(*x>=PS_GRID_COLC)||(*y>=PS_GRID_ROWC)) continue;
    if (grid->cellv[(*y)*PS_GRID_COLC+(*x)].physics!=physics) continue;
    int err=ps_summoner_entry_setup_path_edge_1(grid,x,y,dx,dy,blacklist);
    if (err) return 1;
  }
  
  return 0;
}

static int ps_summoner_entry_setup_path_edge(struct ps_summoner_entry *entry,int x,int y,const struct ps_grid *grid,int dx,int dy) {
  if ((x<0)||(y<0)||(x>=PS_GRID_COLC)||(y>=PS_GRID_ROWC)) return -1;
  uint8_t physics=grid->cellv[y*PS_GRID_COLC+x].physics;
  struct ps_path blacklist={0};
  int err=ps_summoner_entry_setup_path_edge_1(grid,&x,&y,dx,dy,&blacklist);
  ps_path_cleanup(&blacklist);
  if (err<1) return err;

  ps_log(GAME,INFO,"Cell (%d,%d) is on the edge for (%d,%d)",x,y,dx,dy);

  if (ps_path_add(entry->cells,x+dx,y+dy)<0) return -1;
  int donelo=0,donehi=0;
  int add=1; for (;add<PS_GRID_COLC;add++) {
    const struct ps_grid_cell *cell;
    if (dx) {
      if (!donelo) {
        if (y-add<0) {
          donelo=1;
        } else {
          cell=grid->cellv+(y-add)*PS_GRID_COLC+x;
          if (cell->physics==physics) {
            if (ps_path_add(entry->cells,x+dx,y-add)<0) return -1;
          } else donelo=1;
        }
      }
      if (!donehi) {
        if (y+add>=PS_GRID_ROWC) {
          donehi=1;
        } else {
          cell=grid->cellv+(y+add)*PS_GRID_COLC+x;
          if (cell->physics==physics) {
            if (ps_path_add(entry->cells,x+dx,y+add)<0) return -1;
          } else donelo=1;
        }
      }
    } else {
      if (!donelo) {
        if (x-add<0) {
          donelo=1;
        } else {
          cell=grid->cellv+y*PS_GRID_COLC+x-add;
          if (cell->physics==physics) {
            if (ps_path_add(entry->cells,x-add,y+dy)<0) return -1;
          } else donelo=1;
        }
      }
      if (!donehi) {
        if (x+add>=PS_GRID_COLC) {
          donehi=1;
        } else {
          cell=grid->cellv+y*PS_GRID_COLC+x+add;
          if (cell->physics==physics) {
            if (ps_path_add(entry->cells,x+add,y+dy)<0) return -1;
          } else donelo=1;
        }
      }
    }
    if (donelo&&donehi) break;
  }
  
  return 0;
}

/* Populate entry's path, dispatcher.
 */

static int ps_summoner_entry_setup_path(struct ps_summoner_entry *entry,int x,int y,const struct ps_grid *grid) {
  if (x<=2) {
    if (ps_summoner_entry_setup_path_edge(entry,x,y,grid,-1,0)<0) return -1;
  } else if (y<=2) {
    if (ps_summoner_entry_setup_path_edge(entry,x,y,grid,0,-1)<0) return -1;
  } else if (x>=PS_GRID_COLC-3) {
    if (ps_summoner_entry_setup_path_edge(entry,x,y,grid,1,0)<0) return -1;
  } else if (y>=PS_GRID_ROWC-3) {
    if (ps_summoner_entry_setup_path_edge(entry,x,y,grid,0,1)<0) return -1;
  } else {
    if (ps_summoner_entry_setup_path_seed_fill(entry,x,y,grid)<0) return -1;
  }
  return 0;
}

/* Add entry.
 */

static struct ps_summoner_entry *ps_summoner_add_entry(
  struct ps_summoner *summoner,
  struct ps_sprdef *sprdef,
  int volume,
  int x,int y,
  const struct ps_grid *grid
) {

  if (summoner->entryc>=summoner->entrya) {
    int na=summoner->entrya+8;
    if (na>INT_MAX/sizeof(struct ps_summoner_entry)) return 0;
    void *nv=realloc(summoner->entryv,sizeof(struct ps_summoner_entry)*na);
    if (!nv) return 0;
    summoner->entryv=nv;
    summoner->entrya=na;
  }

  struct ps_summoner_entry *entry=summoner->entryv+summoner->entryc++;
  memset(entry,0,sizeof(struct ps_summoner_entry));

  entry->sprdef=sprdef;
  if (volume<1) entry->volume=1;
  else if (volume>100) entry->volume=100;
  else entry->volume=volume;
  entry->spritelimit=ps_summoner_sprite_limit_for_volume(entry->volume);

  if (
    !(entry->cells=calloc(1,sizeof(struct ps_path)))||
    !(entry->alive=ps_sprgrp_new())||
    (ps_summoner_entry_setup_path(entry,x,y,grid)<0)||
    (ps_summoner_entry_reset_delay(entry)<0)
  ) {
    ps_summoner_entry_cleanup(entry);
    summoner->entryc--;
    return 0;
  }

  return entry;
}

/* Reset.
 */

int ps_summoner_reset(struct ps_summoner *summoner,struct ps_game *game) {
  if (!summoner||!game) return -1;

  while (summoner->entryc>0) {
    summoner->entryc--;
    ps_summoner_entry_cleanup(summoner->entryv+summoner->entryc);
  }

  if (game->grid) {
    const struct ps_blueprint_poi *poi=game->grid->poiv;
    int i=game->grid->poic; for (;i-->0;poi++) {
      if (poi->type!=PS_BLUEPRINT_POI_SUMMONER) continue;
      int sprdefid=poi->argv[0];
      int volume=poi->argv[1];

      struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
      if (!sprdef) {
        // This doesn't have to be fatal, but it's bad.
        ps_log(RES,ERROR,"sprdef:%d not found; required by summoner",sprdefid);
        continue;
      }

      struct ps_summoner_entry *entry=ps_summoner_add_entry(summoner,sprdef,volume,poi->x,poi->y,game->grid);
      if (!entry) return -1;
        
    }
  }
  
  return 0;
}

/* Check spawn position. Don't spawn if someone is standing here.
 * No need to check the grid; that's taken care of at reset.
 */

static int ps_summoner_ok_position(const struct ps_game *game,int x,int y) {
  int i=game->grpv[PS_SPRGRP_PHYSICS].sprc;
  while (i-->0) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_PHYSICS].sprv[i];
    int dx=spr->x-x; if (dx<0) dx=-dx;
    if (dx>PS_TILESIZE) continue;
    int dy=spr->y-y; if (dy<0) dy=-dy;
    if (dy>PS_TILESIZE) continue;
    return 0;
  }
  return 1;
}

/* Spawn sprite from entry.
 * It's OK to do nothing; we can apply some extra filtering here.
 */

static int ps_summoner_entry_spawn(struct ps_summoner_entry *entry,struct ps_game *game) {
  if (entry->cells->c<1) return 0;
  if (!entry->sprdef) return 0;
  if (entry->alive->sprc>=entry->spritelimit) return 0;

  int x,y,panic=10;
  while (1) {
    if (--panic<0) return 0;
    int pathp=rand()%entry->cells->c;
    x=(entry->cells->v[pathp].x*PS_TILESIZE)+(PS_TILESIZE>>1);
    y=(entry->cells->v[pathp].y*PS_TILESIZE)+(PS_TILESIZE>>1);
    if (ps_summoner_ok_position(game,x,y)) break;
  }

  struct ps_sprite *spr=ps_sprdef_instantiate(game,entry->sprdef,0,0,x,y);
  if (!spr) return -1;

  if (ps_sprgrp_add_sprite(entry->alive,spr)<0) return -1;
  
  return 0;
}

/* Update.
 */

int ps_summoner_update(struct ps_summoner *summoner,struct ps_game *game) {
  if (!summoner||!game) return -1;
  struct ps_summoner_entry *entry=summoner->entryv;
  int i=summoner->entryc; for (;i-->0;entry++) {
    if (entry->delay>0) {
      entry->delay--;
    } else {
      if (ps_summoner_entry_spawn(entry,game)<0) return -1;
      if (ps_summoner_entry_reset_delay(entry)<0) return -1;
    }
  }
  return 0;
}
