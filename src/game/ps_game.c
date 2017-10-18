#include "ps.h"
#include "ps_game.h"
#include "ps_player.h"
#include "ps_sprite.h"
#include "ps_physics.h"
#include "ps_plrdef.h"
#include "game/sprites/ps_sprite_hero.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_scgen.h"
#include "scenario/ps_world.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "video/ps_video.h"
#include "video/ps_video_layer.h"
#include "input/ps_input.h"

#define PS_PRIZE_SPRDEF_ID 17

/* Game layer.
 */

struct ps_game_layer {
  struct ps_video_layer hdr;
  struct ps_game *game; // WEAK
};

static int ps_game_layer_draw(struct ps_video_layer *layer) {
  struct ps_game *game=((struct ps_game_layer*)layer)->game;
  if (ps_video_draw_grid(game->grid)<0) return -1;
  if (ps_video_draw_sprites(game->grpv+PS_SPRGRP_VISIBLE)<0) return -1;
  return 0;
}

/* Initialize video layers.
 */

static int ps_game_init_video(struct ps_game *game) {

  if (!(game->layer_scene=ps_video_layer_new(sizeof(struct ps_game_layer)))) {
    ps_game_del(game);
    return -1;
  }

  game->layer_scene->blackout=1;
  game->layer_scene->draw=ps_game_layer_draw;
  ((struct ps_game_layer*)game->layer_scene)->game=game;

  if (ps_video_install_layer(game->layer_scene,-1)<0) return -1;

  return 0;
}

/* New game.
 */

struct ps_game *ps_game_new() {
  struct ps_game *game=calloc(1,sizeof(struct ps_game));
  if (!game) return 0;

  game->grpv[PS_SPRGRP_VISIBLE].order=PS_SPRGRP_ORDER_RENDER;

  if (ps_game_init_video(game)<0) {
    ps_game_del(game);
    return 0;
  }

  if (!(game->physics=ps_physics_new())) {
    ps_game_del(game);
    return 0;
  }
  if (ps_physics_set_sprgrp_physics(game->physics,game->grpv+PS_SPRGRP_PHYSICS)<0) {
    ps_game_del(game);
    return 0;
  }
  if (ps_physics_set_sprgrp_solid(game->physics,game->grpv+PS_SPRGRP_SOLID)<0) {
    ps_game_del(game);
    return 0;
  }

  return game;
}

/* Delete.
 */

void ps_game_del(struct ps_game *game) {
  int i;
  if (!game) return;

  ps_video_uninstall_layer(game->layer_scene);
  ps_video_layer_del(game->layer_scene);

  ps_physics_del(game->physics);

  ps_scenario_del(game->scenario);
  while (game->playerc-->0) ps_player_del(game->playerv[game->playerc]);
  for (i=PS_SPRGRP_COUNT;i-->0;) ps_sprgrp_cleanup(game->grpv+i);

  free(game);
}

/* Configure player list.
 */
 
int ps_game_set_player_count(struct ps_game *game,int playerc) {
  if (!game) return -1;
  if ((playerc<1)||(playerc>PS_PLAYER_LIMIT)) return -1;

  while (game->playerc>0) {
    game->playerc--;
    ps_player_del(game->playerv[game->playerc]);
  }

  while (game->playerc<playerc) {
    if (!(game->playerv[game->playerc]=ps_player_new())) return -1;
    game->playerv[game->playerc]->playerid=game->playerc+1;
    game->playerc++;
  }

  if (ps_input_set_player_count(playerc)<0) return -1;

  return 0;
}

int ps_game_set_player_definition(struct ps_game *game,int playerid,int plrdefid) {
  if (!game) return -1;
  if ((playerid<1)||(playerid>game->playerc)) return -1;
  struct ps_plrdef *plrdef=ps_res_get(PS_RESTYPE_PLRDEF,plrdefid);
  if (!plrdef) return -1;
  struct ps_player *player=game->playerv[playerid-1];
  player->plrdef=plrdef;
  player->palette=0;
  return 0;
}

int ps_game_adjust_player_definition(struct ps_game *game,int playerid,int d) {
  if (!game) return -1;
  if ((playerid<1)||(playerid>game->playerc)) return -1;
  const struct ps_restype *restype=PS_RESTYPE(PLRDEF);
  if (!restype||(restype->resc<1)) return -1;
  struct ps_player *player=game->playerv[playerid-1];
  int p;
  if (player->plrdef) {
    if ((p=ps_restype_index_by_object(restype,player->plrdef))<0) p=0;
    else p+=d;
  } else p=0;
  if (p<0) p=restype->resc-1;
  else if (p>=restype->resc) p=0;
  player->plrdef=restype->resv[p].obj;
  player->palette=0;
  return 0;
}

int ps_game_adjust_player_palette(struct ps_game *game,int playerid,int d) {
  if (!game) return -1;
  if ((playerid<1)||(playerid>game->playerc)) return -1;
  struct ps_player *player=game->playerv[playerid-1];
  if (!player->plrdef) return -1;
  int p=player->palette+d;
  if (p<0) p=player->plrdef->palettec-1;
  else if (p>=player->plrdef->palettec) p=0;
  player->palette=0;
  return 0;
}

/* Configure generator.
 */
  
int ps_game_set_difficulty(struct ps_game *game,int difficulty) {
  if (!game) return -1;
  if ((difficulty<PS_DIFFICULTY_MIN)||(difficulty>PS_DIFFICULTY_MAX)) return -1;
  game->difficulty=difficulty;
  return 0;
}

int ps_game_set_length(struct ps_game *game,int length) {
  if (!game) return -1;
  if ((length<PS_LENGTH_MIN)||(length>PS_LENGTH_MAX)) return -1;
  game->length=length;
  return 0;
}

/* Are we configured adequately to generate the scenario?
 */

static int ps_game_ready_to_generate(const struct ps_game *game) {
  if (!game) return 0;

  if ((game->playerc<1)||(game->playerc>PS_PLAYER_LIMIT)) return 0;
  int i; for (i=0;i<game->playerc;i++) {
    struct ps_player *player=game->playerv[i];
    if (!player->plrdef) return 0;
  }

  if ((game->difficulty<PS_DIFFICULTY_MIN)||(game->difficulty>PS_DIFFICULTY_MAX)) return 0;
  if ((game->length<PS_LENGTH_MIN)||(game->length>PS_LENGTH_MAX)) return 0;
  
  return 1;
}

/* Generate scenario.
 */
 
int ps_game_generate(struct ps_game *game) {
  if (!game) return -1;
  if (!ps_game_ready_to_generate(game)) {
    ps_log(RES,ERROR,"%s before ready",__func__);
    return -1;
  }

  if (game->scenario) {
    ps_scenario_del(game->scenario);
    game->scenario=0;
  }

  struct ps_scgen *scgen=ps_scgen_new();
  if (!scgen) return -1;

  scgen->playerc=game->playerc;
  scgen->difficulty=game->difficulty;
  scgen->length=game->length;

  scgen->skills=0;
  int i; for (i=0;i<game->playerc;i++) scgen->skills|=game->playerv[i]->plrdef->skills;

  if (ps_scgen_generate(scgen)<0) {
    ps_log(GENERATOR,ERROR,"Failed to generate scenario: %.*s",scgen->msgc,scgen->msg);
    ps_scgen_del(scgen);
    return -1;
  }

  game->scenario=scgen->scenario;
  scgen->scenario=0;

  ps_scgen_del(scgen);
  return 0;
}

/* Generate test scenario.
 */
 
int _ps_game_generate_test(struct ps_game *game,int regionid,int blueprintid,...) {
  if (!game) return -1;
  if (!ps_game_ready_to_generate(game)) {
    ps_log(RES,ERROR,"%s before ready",__func__);
    return -1;
  }

  if (game->scenario) {
    ps_scenario_del(game->scenario);
    game->scenario=0;
  }

  struct ps_scgen *scgen=ps_scgen_new();
  if (!scgen) return -1;

  scgen->playerc=game->playerc;
  scgen->difficulty=game->difficulty;
  scgen->length=game->length;

  scgen->skills=0;
  int i; for (i=0;i<game->playerc;i++) scgen->skills|=game->playerv[i]->plrdef->skills;

  if (ps_scgen_test_require_region(scgen,regionid)<0) {
    ps_scgen_del(scgen);
    return -1;
  }

  va_list vargs;
  va_start(vargs,blueprintid);
  while (blueprintid>=0) {
    if (ps_scgen_test_require_blueprint(scgen,blueprintid)<0) {
      ps_scgen_del(scgen);
      return -1;
    }
    blueprintid=va_arg(vargs,int);
  }

  if (ps_scgen_test_generate(scgen)<0) {
    ps_log(GENERATOR,ERROR,"Failed to generate scenario: %.*s",scgen->msgc,scgen->msg);
    ps_scgen_del(scgen);
    return -1;
  }

  game->scenario=scgen->scenario;
  scgen->scenario=0;

  ps_scgen_del(scgen);
  return 0;
}

/* Spawn one sprite from a HERO, SPRITE, or TREASURE POI.
 */

static struct ps_sprite *ps_game_spawn_sprite(struct ps_game *game,const struct ps_blueprint_poi *poi) {

  int sprdefid,argc;
  const int *argv;
  switch (poi->type) {
    case PS_BLUEPRINT_POI_HERO: sprdefid=1; argv=poi->argv+1; argc=2; break;
    case PS_BLUEPRINT_POI_SPRITE: sprdefid=poi->argv[0]; argv=poi->argv+1; argc=2; break;
    case PS_BLUEPRINT_POI_TREASURE: sprdefid=12; argv=poi->argv; argc=3; break;
    default: return 0;
  }
  
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
  if (!sprdef) {
    ps_log(GAME,ERROR,"sprdef:%d not found",sprdefid);
    return 0;
  }
  int x=poi->x*PS_TILESIZE+(PS_TILESIZE>>1);
  int y=poi->y*PS_TILESIZE+(PS_TILESIZE>>1);
  struct ps_sprite *sprite=ps_sprdef_instantiate(game,sprdef,argv,argc,x,y);
  if (!sprite) {
    ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",poi->argv[0]);
    return 0;
  }
  return sprite;
}

/* Spawn sprites for fresh grid.
 */

static const struct ps_blueprint_poi *ps_game_get_poi_for_hero(const struct ps_game *game,int playerid) {
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  int i=game->grid->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_HERO) {
      if (poi->argv[0]==playerid) {
        return poi;
      }
    }
  }
  return 0;
}

static int ps_game_spawn_hero_sprites(struct ps_game *game) {
  if (!game) return -1;
  int i=1; for (;i<=game->playerc;i++) {
    const struct ps_blueprint_poi *poi=ps_game_get_poi_for_hero(game,i);
    if (!poi) {
      ps_log(GAME,ERROR,"Start point for player %d not found.",i);
      return -1;
    }
    struct ps_sprite *spr=ps_game_spawn_sprite(game,poi);
    if (!spr) return -1;
    if (ps_hero_set_player(spr,game->playerv[i-1])<0) return -1;
  }
  return 0;
}

static int ps_game_spawn_sprites(struct ps_game *game) {
  if (!game) return -1;
  
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  int i=game->grid->poic;
  for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_SPRITE) {
      struct ps_sprite *sprite=ps_game_spawn_sprite(game,poi);
      if (!sprite) return -1;
    } else if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
      struct ps_sprite *sprite=ps_game_spawn_sprite(game,poi);
      if (!sprite) return -1;
    }
  }

  return 0;
}

/* Restart.
 */

int ps_game_restart(struct ps_game *game) {
  if (!game||!game->scenario) return -1;

  ps_log(GAME,DEBUG,"Restarting game at home screen (%d,%d) of (%d,%d).",
    game->scenario->homex,game->scenario->homey,game->scenario->world->w,game->scenario->world->h
  );

  memset(game->treasurev,0,sizeof(game->treasurev));
  game->treasurec=game->scenario->treasurec;

  game->gridx=game->scenario->homex;
  game->gridy=game->scenario->homey;
  game->grid=PS_WORLD_SCREEN(game->scenario->world,game->gridx,game->gridy)->grid;
  if (ps_grid_close_all_barriers(game->grid)<0) return -1;
  if (ps_physics_set_grid(game->physics,game->grid)<0) return -1;

  ps_sprgrp_kill(game->grpv+PS_SPRGRP_KEEPALIVE);

  if (ps_game_spawn_hero_sprites(game)<0) return -1;
  if (ps_game_spawn_sprites(game)<0) return -1;

  return 0;
}

/* Kill every sprite except those in the HERO group.
 */

static int ps_game_kill_nonhero_sprites(struct ps_game *game) {
  int i; for (i=game->grpv[PS_SPRGRP_KEEPALIVE].sprc;i-->0;) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_KEEPALIVE].sprv[i];
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HERO,spr)) continue;
    if (ps_sprgrp_add_sprite(game->grpv+PS_SPRGRP_DEATHROW,spr)) continue;
  }
  if (ps_sprgrp_kill(game->grpv+PS_SPRGRP_DEATHROW)<0) return -1;
  return 0;
}

/* Change to a neighboring grid.
 */

static int ps_game_load_neighbor_grid(struct ps_game *game,struct ps_grid *grid,int dx,int dy) {
  game->grid=grid;
  game->gridx+=dx;
  game->gridy+=dy;
  if (ps_grid_close_all_barriers(game->grid)<0) return -1;
  if (ps_physics_set_grid(game->physics,game->grid)<0) return -1;
  if (ps_game_kill_nonhero_sprites(game)<0) return -1;

  /* Adjust position of heroes. */
  double sprdx=-dx*PS_SCREENW;
  double sprdy=-dy*PS_SCREENH;
  int i=game->grpv[PS_SPRGRP_HERO].sprc; while (i-->0) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_HERO].sprv[i];
    spr->x+=sprdx;
    spr->y+=sprdy;
  }

  /* Create sprites for new grid. */
  if (ps_game_spawn_sprites(game)<0) return -1;

  ps_log(GAME,DEBUG,"Switch to grid (%d,%d), blueprint:%d",game->gridx,game->gridy,ps_game_get_current_blueprint_id(game));
  
  return 0;
}

/* If every hero is offscreen in the same direction, switch to another grid.
 */

static int ps_game_get_sprite_grid_change(const struct ps_sprite *spr) {
  if (spr->x+spr->radius<=0.0) return PS_DIRECTION_WEST;
  if (spr->y+spr->radius<=0.0) return PS_DIRECTION_NORTH;
  if (spr->x-spr->radius>=PS_SCREENW) return PS_DIRECTION_EAST;
  if (spr->y-spr->radius>=PS_SCREENH) return PS_DIRECTION_SOUTH;
  return 0;
}

static int ps_game_check_grid_change(struct ps_game *game) {

  /* Short-circuit if no heroes. I don't think this is a valid situation, but whatever. */
  if (game->grpv[PS_SPRGRP_HERO].sprc<1) return 0;

  /* Check changeability of the first sprite, then ensure that others match. */
  int changedir=ps_game_get_sprite_grid_change(game->grpv[PS_SPRGRP_HERO].sprv[0]);
  if (!changedir) return 0;
  int i=game->grpv[PS_SPRGRP_HERO].sprc; while (i-->1) {
    if (ps_game_get_sprite_grid_change(game->grpv[PS_SPRGRP_HERO].sprv[i])!=changedir) return 0;
  }

  /* OK, we are changing. */
  struct ps_vector d=ps_vector_from_direction(changedir);
  int nx=game->gridx+d.dx;
  int ny=game->gridy+d.dy;
  if ((nx<0)||(ny<0)||(nx>=game->scenario->world->w)||(ny>=game->scenario->world->h)) return 0;
  struct ps_grid *ngrid=game->scenario->world->v[ny*game->scenario->world->w+nx].grid;
  
  return ps_game_load_neighbor_grid(game,ngrid,d.dx,d.dy);
}

/* Forcibly move to a neighbor grid.
 */

static int ps_game_hero_in_ok_position(struct ps_game *game,struct ps_sprite *hero) {
  int col=hero->x/PS_TILESIZE; if ((col<0)||(col>=PS_GRID_COLC)) return 0;
  int row=hero->y/PS_TILESIZE; if ((row<0)||(row>=PS_GRID_ROWC)) return 0;
  const struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
  if (cell->physics!=PS_BLUEPRINT_CELL_VACANT) return 0;
  // We could check other sprites, but meh.
  return 1;
}

static void ps_game_force_hero_to_legal_position(struct ps_game *game,struct ps_sprite *hero) {

  /* First try dropping him at the same position in the new screen. That would be least jarring to the user. */
  while (hero->x<0.0) hero->x+=PS_SCREENW;
  while (hero->x>=PS_SCREENW) hero->x-=PS_SCREENW;
  while (hero->y<0.0) hero->y+=PS_SCREENH;
  while (hero->y>=PS_SCREENH) hero->y-=PS_SCREENH;
  if (ps_game_hero_in_ok_position(game,hero)) return;

  /* Put him on random cells until we find a good one, or a sanity limit expires.
   * This behavior is debug-only, so don't freak out over it.
   */
  int limit=100;
  while (limit-->0) {
    int col=rand()%PS_GRID_COLC;
    int row=rand()%PS_GRID_ROWC;
    hero->x=col*PS_TILESIZE+(PS_TILESIZE>>1);
    hero->y=row*PS_TILESIZE+(PS_TILESIZE>>1);
    if (ps_game_hero_in_ok_position(game,hero)) return;
  }
}

int ps_game_force_next_screen(struct ps_game *game,int dx,int dy) {
  if (!game) return -1;
  
  int nx=game->gridx+dx;
  int ny=game->gridy+dy;
  if ((nx<0)||(ny<0)||(nx>=game->scenario->world->w)||(ny>=game->scenario->world->h)) return 0;
  struct ps_grid *ngrid=game->scenario->world->v[ny*game->scenario->world->w+nx].grid;
  
  if (ps_game_load_neighbor_grid(game,ngrid,dx,dy)<0) return -1;

  /* Normally, all the heroes would already be on this neighboring grid.
   * That's not the case for us. Force them in.
   */
  int i; for (i=0;i<game->grpv[PS_SPRGRP_HERO].sprc;i++) {
    struct ps_sprite *hero=game->grpv[PS_SPRGRP_HERO].sprv[i];
    if ((hero->x<0.0)||(hero->y<0.0)||(hero->x>PS_SCREENW)||(hero->y>PS_SCREENH)) {
      ps_game_force_hero_to_legal_position(game,hero);
    }
  }
  
  return 1;
}

/* Look for collisions between HAZARD and FRAGILE sprites, and take appropriate action.
 */

static int ps_game_check_for_damage(struct ps_game *game) {

  int i=0; for (;i<game->grpv[PS_SPRGRP_HAZARD].sprc;i++) {
    struct ps_sprite *hazard=game->grpv[PS_SPRGRP_HAZARD].sprv[i];
    int j=0; for (;j<game->grpv[PS_SPRGRP_FRAGILE].sprc;j++) {
      struct ps_sprite *fragile=game->grpv[PS_SPRGRP_FRAGILE].sprv[j];
      if (hazard==fragile) continue; // Perfectly normal for a sprite to be both HAZARD and FRAGILE.
      if (ps_sprites_collide(hazard,fragile)) {
        if (ps_sprite_receive_damage(game,fragile,hazard)<0) return -1;
      }
    }
  }

  for (i=0;i<game->grpv[PS_SPRGRP_HEROHAZARD].sprc;i++) {
    struct ps_sprite *hazard=game->grpv[PS_SPRGRP_HEROHAZARD].sprv[i];
    int j=0; for (;j<game->grpv[PS_SPRGRP_HERO].sprc;j++) {
      struct ps_sprite *fragile=game->grpv[PS_SPRGRP_HERO].sprv[j];
      if (hazard==fragile) continue;
      if (ps_sprites_collide(hazard,fragile)) {
        if (ps_sprite_receive_damage(game,fragile,hazard)<0) return -1;
      }
    }
  }
  
  return 0;
}

/* Look for damaging collisions among the physics's events.
 */

static int ps_game_check_physics_for_damage(struct ps_game *game) {
  struct ps_physics_event *event=game->physics->eventv;
  int i=game->physics->eventc; for (;i-->0;event++) {
    if (!event->a||!event->b) continue;
    
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HAZARD,event->a)) {
      if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_FRAGILE,event->b)) {
        if (ps_sprite_receive_damage(game,event->b,event->a)<0) return -1;
        continue;
      }
    }
    
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HAZARD,event->b)) {
      if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_FRAGILE,event->a)) {
        if (ps_sprite_receive_damage(game,event->a,event->b)<0) return -1;
        continue;
      }
    }
    
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,event->a)) {
      if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HERO,event->b)) {
        if (ps_sprite_receive_damage(game,event->b,event->a)<0) return -1;
        continue;
      }
    }
    
    if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HEROHAZARD,event->b)) {
      if (ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_HERO,event->a)) {
        if (ps_sprite_receive_damage(game,event->a,event->b)<0) return -1;
        continue;
      }
    }
    
  }
  return 0;
}

/* Check completion.
 */

static int ps_game_check_completion(struct ps_game *game) {

  if (!game->treasurec) {
    // This is a special case that can only happen when we generated a "test" grid.
    // Just carry on; the game is unwinnable.
    return 0;
  }

  int i; for (i=0;i<game->treasurec;i++) {
    if (!game->treasurev[i]) return 0;
  }
  //TODO What to do when game complete?
  ps_log(GAME,INFO,"Game complete!");
  return ps_game_restart(game);
}

/* Update.
 */

int ps_game_update(struct ps_game *game) {
  if (!game) return -1;

  /* Update sprites. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=0; for (i=0;i<grp->sprc;i++) {
    if (ps_sprite_update(grp->sprv[i],game)<0) return -1;
  }

  /* Update physics, then consider any hazardous collisions. */
  if (ps_physics_update(game->physics)<0) return -1;
  if (ps_game_check_physics_for_damage(game)<0) return -1;

  /* Look for collisions between HAZARD and FRAGILE sprites.
   * Some damage methods, eg sword, are managed by individual sprite types.
   */
  if (ps_game_check_for_damage(game)<0) return -1;

  /* Clear death row. */
  if (ps_sprgrp_kill(game->grpv+PS_SPRGRP_DEATHROW)<0) return -1;

  /* Change grid if warranted. */
  if (ps_game_check_grid_change(game)<0) return -1;

  /* Sort rendering group, one pass. */
  if (ps_sprgrp_sort(game->grpv+PS_SPRGRP_VISIBLE,0)<0) return -1;

  /* Check for completion. */
  if (ps_game_check_completion(game)<0) return -1;

  return 0;
}

/* Pause.
 */
 
int ps_game_toggle_pause(struct ps_game *game) {
  if (!game) return -1;
  ps_log(GAME,INFO,"TODO: Pause/resume");
  return 0;
}

/* Current screen or blueprint.
 */
 
struct ps_screen *ps_game_get_screen_for_current_grid(const struct ps_game *game) {
  if (!game) return 0;
  if (!game->scenario||!game->scenario->world) return 0;
  if ((game->gridx<0)||(game->gridx>=game->scenario->world->w)) return 0;
  if ((game->gridy<0)||(game->gridy>=game->scenario->world->h)) return 0;
  return game->scenario->world->v+(game->gridy*game->scenario->world->w)+game->gridx;
}

int ps_game_get_current_blueprint_id(const struct ps_game *game) {
  struct ps_screen *screen=ps_game_get_screen_for_current_grid(game);
  if (!screen) return -1;
  return ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,screen->blueprint);
}

/* Collect treasure.
 */
 
int ps_game_collect_treasure(struct ps_game *game,struct ps_sprite *collector,int treasureid) {
  if (!game) return -1;

  if ((treasureid<0)||(treasureid>=game->treasurec)) {
    ps_log(GAME,ERROR,"Collected treasure id %d, global count is %d.",treasureid,game->treasurec);
    return 0;
  }
  if (game->treasurev[treasureid]) return 0; // Already have it. Huh.
  
  game->treasurev[treasureid]=1;

  return 1;
}

int ps_game_get_treasure_state(const struct ps_game *game,int treasureid) {
  if (!game) return 0;
  if ((treasureid<0)||(treasureid>=game->treasurec)) return 0;
  return game->treasurev[treasureid];
}

/* Create fireworks when something dies.
 */
 
int ps_game_create_fireworks(struct ps_game *game,int x,int y) {
  if (!game) return -1;
  //TODO fireworks
  return 0;
}

/* Create prize when something dies.
 */
 
int ps_game_create_prize(struct ps_game *game,int x,int y) {
  if (!game) return -1;
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_PRIZE_SPRDEF_ID);
  if (!sprdef) return -1;
  struct ps_sprite *prize=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
  if (!prize) return -1;
  return 0;
}
