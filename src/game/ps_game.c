#include "ps.h"
#include "ps_game.h"
#include "ps_player.h"
#include "ps_sprite.h"
#include "ps_physics.h"
#include "ps_plrdef.h"
#include "ps_stats.h"
#include "ps_bloodhound_activator.h"
#include "ps_statusreport.h"
#include "ps_sound_effects.h"
#include "ps_dragoncharger.h"
#include "game/sprites/ps_sprite_hero.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_scgen.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_region.h"
#include "scenario/ps_screen.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "video/ps_video.h"
#include "video/ps_video_layer.h"
#include "input/ps_input.h"
#include "util/ps_enums.h"

#define PS_PRIZE_SPRDEF_ID 17
#define PS_SPLASH_SPRDEF_ID 24

static int ps_game_npgc_pop(struct ps_game *game);

/* Game layer.
 */

struct ps_game_layer {
  struct ps_video_layer hdr;
  struct ps_game *game; // WEAK
};

static int ps_game_draw_hud(struct ps_game *game) {
  if (ps_video_text_begin()<0) return -1;
  if (ps_video_text_addf(12,0x000000ff,6,7,
    "%d/%d  %02d:%02d",
    ps_game_count_collected_treasures(game),game->treasurec,
    game->stats->playtime/3600,(game->stats->playtime/60)%60
  )<0) return -1;
  if (ps_video_text_end(0)<0) return -1;
  return 0;
}

static int ps_game_layer_draw(struct ps_video_layer *layer) {
  struct ps_game *game=((struct ps_game_layer*)layer)->game;
  if (game&&game->grid) {
    if (ps_video_draw_grid(game->grid)<0) return -1;
    if (game->statusreport) {
      if (ps_statusreport_draw(game->statusreport)<0) return -1;
    }
    if (ps_video_draw_sprites(game->grpv+PS_SPRGRP_VISIBLE)<0) return -1;
    if (ps_game_draw_hud(game)<0) return -1;
  }
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

static int ps_game_initialize(struct ps_game *game) {

  game->grpv[PS_SPRGRP_VISIBLE].order=PS_SPRGRP_ORDER_RENDER;

  if (!(game->stats=ps_stats_new())) return -1;
  
  if (ps_game_init_video(game)<0) return -1;

  if (!(game->physics=ps_physics_new())) return -1;
  if (ps_physics_set_sprgrp_physics(game->physics,game->grpv+PS_SPRGRP_PHYSICS)<0) return -1;
  if (ps_physics_set_sprgrp_solid(game->physics,game->grpv+PS_SPRGRP_SOLID)<0) return -1;
  if (ps_physics_set_sprgrp_hero(game->physics,game->grpv+PS_SPRGRP_HERO)<0) return -1;
    
  if (!(game->bloodhound_activator=ps_bloodhound_activator_new())) return -1;
  if (!(game->dragoncharger=ps_dragoncharger_new())) return -1;

  return 0;
}

struct ps_game *ps_game_new() {
  struct ps_game *game=calloc(1,sizeof(struct ps_game));
  if (!game) return 0;
  if (ps_game_initialize(game)<0) {
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
  ps_statusreport_del(game->statusreport);
  ps_dragoncharger_del(game->dragoncharger);
  ps_bloodhound_activator_del(game->bloodhound_activator);

  ps_scenario_del(game->scenario);
  while (game->playerc-->0) ps_player_del(game->playerv[game->playerc]);
  for (i=PS_SPRGRP_COUNT;i-->0;) ps_sprgrp_cleanup(game->grpv+i);
  ps_stats_del(game->stats);
  if (game->npgcv) free(game->npgcv);
  
  free(game);
}

/* Set player count.
 */
 
int ps_game_set_player_count(struct ps_game *game,int playerc) {
  if (!game) return -1;
  if ((playerc<0)||(playerc>PS_PLAYER_LIMIT)) return -1;

  while (game->playerc>playerc) {
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

/* Configure player.
 */
 
int ps_game_configure_player(struct ps_game *game,int playerid,int plrdefid,int palette,struct ps_input_device *device) {
  if (!game) return -1;
  if ((playerid<1)||(playerid>game->playerc)) return -1;
  struct ps_player *player=game->playerv[playerid-1];
  player->playerid=playerid;

  if (!(player->plrdef=ps_res_get(PS_RESTYPE_PLRDEF,plrdefid))) {
    ps_log(GAME,ERROR,"plrdef:%d not found",plrdefid);
    return -1;
  }

  if (player->plrdef->palettec<1) {
    ps_log(GAME,ERROR,"Invalid plrdef, id %d",plrdefid);
    return -1;
  }
  if (palette<0) palette=0;
  else if (palette>=player->plrdef->palettec) palette%=player->plrdef->palettec;
  player->palette=palette;

  if (device) {
    if (ps_input_force_device_assignment(device,playerid)<0) return -1;
  }
  
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

  ps_log(GAME,DEBUG,"%s playerc=%d difficulty=%d length=%d",__func__,game->playerc,game->difficulty,game->length);

  if ((game->playerc<1)||(game->playerc>PS_PLAYER_LIMIT)) return 0;
  int i; for (i=0;i<game->playerc;i++) {
    struct ps_player *player=game->playerv[i];
    ps_log(GAME,DEBUG,"  %d: plrdef=%p",i+1,player->plrdef);
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
  game->npgcc=0; // Don't pop them; we just deleted the grids.

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

/* About to spawn a random sprite, select a position for it.
 * Failures here are not fatal.
 */

static int ps_game_sprite_position_conflicts_with_others(const struct ps_game *game,int x,int y) {
  // We'll use all sprites for this test. Maybe we should restrict to PHYSICS?
  const struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_KEEPALIVE;
  int i=grp->sprc; while (i-->0) {
    const struct ps_sprite *spr=grp->sprv[i];
    int dx=x-spr->x;
    if (dx>=PS_TILESIZE) continue;
    if (dx<=-PS_TILESIZE) continue;
    int dy=y-spr->y;
    if (dy>=PS_TILESIZE) continue;
    if (dy<=-PS_TILESIZE) continue;
  }
  return 0;
}

static int ps_game_select_position_for_random_sprite(int *x,int *y,const struct ps_game *game,const struct ps_sprdef *sprdef) {
  const int margin=2; // Don't spawn on the edges.
  int attemptc=20;
  while (attemptc-->0) {
    int col=margin+(rand()%(PS_GRID_COLC-(margin<<1)));
    int row=margin+(rand()%(PS_GRID_ROWC-(margin<<1)));
    uint8_t physics=game->grid->cellv[row*PS_GRID_COLC+col].physics;
    if (physics!=PS_BLUEPRINT_CELL_VACANT) continue; // Only spawn on vacant cells.
    *x=col*PS_TILESIZE+(PS_TILESIZE>>1);
    *y=row*PS_TILESIZE+(PS_TILESIZE>>1);
    if (ps_game_sprite_position_conflicts_with_others(game,*x,*y)) continue; // Don't crowd other sprites.
    return 0;
  }
  return -1;
}

/* Spawn sprites from the region's monster menu, if present.
 */

static int ps_game_spawn_random_sprites(struct ps_game *game) {
  if (!game) return -1;
  if (!game->grid) return -1;
  if (!game->grid->region) return 0; // Grids are allowed to not have a region (but they always will)

  if (!game->grid->monsterc_max) return 0; // No monsters here please.
  if (game->grid->monsterc_min>game->grid->monsterc_max) return -1;

  int defc=ps_region_count_monsters_at_difficulty(game->grid->region,game->difficulty);
  if (defc<1) return 0; // Perfectly fine if the region doesn't want random monsters.

  int monsterc_min=game->grid->monsterc_min;
  int monsterc_max=game->grid->monsterc_max;
  int monsterc=monsterc_min+rand()%(monsterc_max-monsterc_min+1);
  if (monsterc<1) return 0;

  int i=monsterc; while (i-->0) {
    int defp=rand()%defc;
    int sprdefid=ps_region_get_monster_at_difficulty(game->grid->region,defp,game->difficulty);
    if (sprdefid<1) {
      ps_log(GAME,ERROR,"Unexpected error: ps_region_count_monsters()==%d, ps_region_get_monster(%d)==%d",defc,defp,sprdefid);
      return -1;
    }
    struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
    if (!sprdef) {
      ps_log(GAME,ERROR,"sprdef:%d not found",sprdefid);
      return -1;
    }
    int x,y;
    if (ps_game_select_position_for_random_sprite(&x,&y,game,sprdef)<0) {
      // It could be that the screen is overpopulated. In this case, skip it.
      continue;
    }
    struct ps_sprite *sprite=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
    if (!sprite) {
      ps_log(GAME,ERROR,"Failed to instantiate sprdef:%d",sprdefid);
      return -1;
    }
  }

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

  /* Spawn from POI records. */
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

  /* Spawn randomly if configured to. */
  if (ps_game_spawn_random_sprites(game)<0) return -1;

  return 0;
}

/* Death gates.
 */

static int ps_game_monsters_present(const struct ps_game *game) {
  int i=game->grpv[PS_SPRGRP_HEROHAZARD].sprc;
  while (i-->0) {
    struct ps_sprite *spr=game->grpv[PS_SPRGRP_HEROHAZARD].sprv[i];
    if (!ps_sprgrp_has_sprite(game->grpv+PS_SPRGRP_DEATHROW,spr)) return 1;
  }
  return 0;
}

static int ps_game_setup_deathgate(struct ps_game *game) {
  if (!game||!game->grid) return -1;
  int i=game->grid->poic;
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  int monsters_present=0; // -1=no, 1=yes, 0=unset
  for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_DEATHGATE) {
      if (!monsters_present) monsters_present=(ps_game_monsters_present(game)?1:-1);
      struct ps_grid_cell *cell=game->grid->cellv+poi->y*PS_GRID_COLC+poi->x;
      if (monsters_present>0) {
        cell->tileid=poi->argv[0];
        cell->physics=PS_BLUEPRINT_CELL_SOLID;
      } else {
        cell->tileid=poi->argv[1];
        cell->physics=PS_BLUEPRINT_CELL_VACANT;
      }
    }
  }
  return 0;
}

/* Rebuild status report or kill it.
 */

static struct ps_blueprint_poi *ps_game_find_status_report_poi(const struct ps_game *game) {
  if (!game->grid) return 0;
  struct ps_blueprint_poi *poi=game->grid->poiv;
  int i=game->grid->poic; for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_STATUSREPORT) return poi;
  }
  return 0;
}

static int ps_game_check_status_report(struct ps_game *game) {

  if (game->statusreport) {
    ps_statusreport_del(game->statusreport);
    game->statusreport=0;
  }
  
  const struct ps_blueprint_poi *poi=ps_game_find_status_report_poi(game);
  if (!poi) return 0;

  int x,y,w,h;
  if (ps_game_get_contiguous_physical_rect_in_grid(&x,&y,&w,&h,game->grid,poi->x,poi->y)<0) return -1;

  if (!(game->statusreport=ps_statusreport_new())) return -1;
  if (ps_statusreport_setup(game->statusreport,game,x,y,w,h)<0) {
    ps_log(GAME,ERROR,"Failed to initialize status report.");
    ps_statusreport_del(game->statusreport);
    game->statusreport=0;
    return -1;
  }
  
  return 0;
}

/* Hard restart.
 */

int ps_game_restart(struct ps_game *game) {
  if (!game||!game->scenario) return -1;

  ps_log(GAME,DEBUG,"Restarting game at home screen (%d,%d) of (%d,%d).",
    game->scenario->homex,game->scenario->homey,game->scenario->w,game->scenario->h
  );

  PS_SFX_BEGIN_PLAY

  game->finished=0;
  game->paused=0;
  if (ps_stats_clear(game->stats)<0) return -1;
  ps_game_npgc_pop(game); // Just to be safe.

  memset(game->treasurev,0,sizeof(game->treasurev));
  game->treasurec=game->scenario->treasurec;

  game->gridx=game->scenario->homex;
  game->gridy=game->scenario->homey;
  game->grid=PS_SCENARIO_SCREEN(game->scenario,game->gridx,game->gridy)->grid;
  game->grid->visited=1;
  if (ps_grid_close_all_barriers(game->grid)<0) return -1;
  if (ps_physics_set_grid(game->physics,game->grid)<0) return -1;

  ps_sprgrp_kill(game->grpv+PS_SPRGRP_KEEPALIVE);

  if (ps_game_spawn_hero_sprites(game)<0) return -1;
  if (ps_game_spawn_sprites(game)<0) return -1;
  if (ps_game_setup_deathgate(game)<0) return -1;

  if (game->grid->region) {
    if (akau_play_song(game->grid->region->songid,0)<0) return -1;
  }

  if (ps_bloodhound_activator_reset(game->bloodhound_activator)<0) return -1;
  if (ps_game_check_status_report(game)<0) return -1;

  return 0;
}

/* Soft restart.
 */
 
int ps_game_return_to_start_screen(struct ps_game *game) {
  if (!game||!game->scenario) return -1;

  ps_log(GAME,DEBUG,"Returning to home screen (%d,%d) of (%d,%d).",
    game->scenario->homex,game->scenario->homey,game->scenario->w,game->scenario->h
  );

  PS_SFX_BEGIN_PLAY

  game->finished=0;
  game->paused=0;
  ps_game_npgc_pop(game);

  game->gridx=game->scenario->homex;
  game->gridy=game->scenario->homey;
  game->grid=PS_SCENARIO_SCREEN(game->scenario,game->gridx,game->gridy)->grid;
  if (ps_grid_close_all_barriers(game->grid)<0) return -1;
  if (ps_physics_set_grid(game->physics,game->grid)<0) return -1;

  ps_sprgrp_kill(game->grpv+PS_SPRGRP_KEEPALIVE);

  if (ps_game_spawn_hero_sprites(game)<0) return -1;
  if (ps_game_spawn_sprites(game)<0) return -1;
  if (ps_game_setup_deathgate(game)<0) return -1;

  if (game->grid->region) {
    if (akau_play_song(game->grid->region->songid,0)<0) return -1;
  }
  
  if (ps_game_check_status_report(game)<0) return -1;

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

  PS_SFX_SHIFT_SCREEN

  ps_game_npgc_pop(game);
  game->grid=grid;
  game->gridx+=dx;
  game->gridy+=dy;
  game->grid->visited=1;
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

  /* Check death gates. */
  if (ps_game_setup_deathgate(game)<0) return -1;

  if (game->grid->region) {
    if (akau_play_song(game->grid->region->songid,0)<0) return -1;
  }
  
  if (ps_game_check_status_report(game)<0) return -1;

  ps_log(GAME,DEBUG,"Switch to grid (%d,%d), blueprint:%d",game->gridx,game->gridy,ps_game_get_current_blueprint_id(game));
  
  return 0;
}

/* If every hero is offscreen in the same direction, switch to another grid.
 */

static int ps_game_get_sprite_grid_change(const struct ps_sprite *spr) {
  if (spr->x<=0.0) return PS_DIRECTION_WEST;
  if (spr->y<=0.0) return PS_DIRECTION_NORTH;
  if (spr->x>=PS_SCREENW) return PS_DIRECTION_EAST;
  if (spr->y>=PS_SCREENH) return PS_DIRECTION_SOUTH;
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
  if ((nx<0)||(ny<0)||(nx>=game->scenario->w)||(ny>=game->scenario->h)) return 0;
  struct ps_grid *ngrid=game->scenario->screenv[ny*game->scenario->w+nx].grid;
  
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
  if ((nx<0)||(ny<0)||(nx>=game->scenario->w)||(ny>=game->scenario->h)) return 0;
  struct ps_grid *ngrid=game->scenario->screenv[ny*game->scenario->w+nx].grid;
  
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

  PS_SFX_VICTORY
  ps_log(GAME,INFO,"Game complete!");
  game->finished=1;
  return 0;
}

/* Update stats, polling for routine events.
 */

static int ps_game_update_stats(struct ps_game *game) {

  game->stats->playtime++;
  game->stats->framec_since_treasure++;
  
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type!=&ps_sprtype_hero) continue;
    struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)hero;
    if (!HERO->player) continue;
    if ((HERO->player->playerid<1)||(HERO->player->playerid>PS_PLAYER_LIMIT)) continue;
    struct ps_stats_player *pstats=game->stats->playerv+HERO->player->playerid-1;
    
    if (HERO->walk_in_progress) {
      pstats->stepc++;
    }
    if (HERO->hp) {
      pstats->framec_alive++;
      pstats->framec_since_rebirth++;
      if (HERO->walk_in_progress) {
        pstats->stepc_since_rebirth++;
      }
    } else {
      pstats->framec_since_rebirth=0;
      pstats->stepc_since_rebirth=0;
    }
    
  }
  
  return 0;
}

/* Update.
 */

int ps_game_update(struct ps_game *game) {
  if (!game) return -1;

  if (game->finished) return 0;

  /* Check for PAUSE key from any player. */
  uint16_t input=ps_get_player_buttons(0);
  if (input&PS_PLRBTN_START) {
    if (ps_game_pause(game,1)<0) return -1;
    return 0;
  }

  /* Externalized game logic. */
  if (ps_bloodhound_activator_update(game->bloodhound_activator,game)<0) return -1;
  if (ps_dragoncharger_update(game->dragoncharger,game)<0) return -1;

  /* Update sprites. */
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_UPDATE;
  int i=0; for (i=0;i<grp->sprc;i++) {
    if (ps_sprite_update(grp->sprv[i],game)<0) return -1;
  }

  /* Poll routine events and record stats. */
  if (ps_game_update_stats(game)<0) return -1;

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

int ps_game_pause(struct ps_game *game,int pause) {
  if (!game) return -1;
  if (pause) {
    if (game->paused) return 0;
    game->paused=1;
    PS_SFX_PAUSE
  } else {
    if (!game->paused) return 0;
    game->paused=0;
    PS_SFX_PAUSE
  }
  return 0;
}

/* Current screen or blueprint.
 */
 
struct ps_screen *ps_game_get_screen_for_current_grid(const struct ps_game *game) {
  if (!game) return 0;
  if (!game->scenario) return 0;
  if ((game->gridx<0)||(game->gridx>=game->scenario->w)) return 0;
  if ((game->gridy<0)||(game->gridy>=game->scenario->h)) return 0;
  return game->scenario->screenv+(game->gridy*game->scenario->w)+game->gridx;
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

  PS_SFX_TREASURE
  
  game->treasurev[treasureid]=1;
  game->stats->framec_since_treasure=0;

  if (collector&&(collector->type==&ps_sprtype_hero)) {
    struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)collector;
    if (HERO->player&&(HERO->player->playerid>=1)&&(HERO->player->playerid<=PS_PLAYER_LIMIT)) {
      struct ps_stats_player *pstats=game->stats->playerv+HERO->player->playerid-1;
      pstats->treasurec++;
    }
  }

  const struct ps_res_trdef *trdef=0;
  if (game->scenario&&(treasureid<game->scenario->treasurec)) trdef=game->scenario->treasurev[treasureid];
  if (trdef) {
    ps_log(GAME,INFO,"Collected treasure '%.*s'.",trdef->namec,trdef->name);
    game->got_treasure=trdef;
  }

  return 1;
}

int ps_game_get_treasure_state(const struct ps_game *game,int treasureid) {
  if (!game) return 0;
  if ((treasureid<0)||(treasureid>=game->treasurec)) return 0;
  return game->treasurev[treasureid];
}

int ps_game_count_collected_treasures(const struct ps_game *game) {
  if (!game) return 0;
  int c=0,i=game->treasurec;
  while (i-->0) if (game->treasurev[i]) c++;
  return c;
}

/* Report statistics for kill.
 */
 
int ps_game_report_kill(struct ps_game *game,struct ps_sprite *assailant,struct ps_sprite *victim) {
  if (!game||!assailant||!victim) return 0;
  
  if (assailant->type==&ps_sprtype_hero) {
    struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)assailant;
    if (HERO->player&&(HERO->player->playerid>=1)&&(HERO->player->playerid<=PS_PLAYER_LIMIT)) {
      struct ps_stats_player *pstats=game->stats->playerv+HERO->player->playerid-1;
      if (victim->type==&ps_sprtype_hero) {
        pstats->killc_hero++;
      } else {
        pstats->killc_monster++;
      }
    }
  }

  return 0;
}

int ps_game_report_switch(struct ps_game *game,struct ps_sprite *presser) {
  if (!game||!presser) return 0;
  if (presser->type==&ps_sprtype_hero) {
    struct ps_sprite_hero *HERO=(struct ps_sprite_hero*)presser;
    if (HERO->player&&(HERO->player->playerid>=1)&&(HERO->player->playerid<=PS_PLAYER_LIMIT)) {
      struct ps_stats_player *pstats=game->stats->playerv+HERO->player->playerid-1;
      pstats->switchc++;
    }
  }
  return 0;
}

/* Dump statistics.
 */
 
void ps_game_dump_stats(const struct ps_game *game) {
  if (!game) return;
  ps_stats_dump(game->stats);
}

/* Create fireworks when something dies.
 */

#define PS_FIREWORKS_SPRITE_COUNT 20

int ps_game_create_fireworks(struct ps_game *game,int x,int y) {
  if (!game) return -1;

  int i=PS_FIREWORKS_SPRITE_COUNT; while (i-->0) {
    if (!ps_sprite_fireworks_new(game,x,y,i,PS_FIREWORKS_SPRITE_COUNT)) return -1;
  }
  
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

/* Create splash when something falls in the water.
 */
 
int ps_game_create_splash(struct ps_game *game,int x,int y) {
  if (!game) return -1;
  struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,PS_SPLASH_SPRDEF_ID);
  if (!sprdef) return -1;
  struct ps_sprite *splash=ps_sprdef_instantiate(game,sprdef,0,0,x,y);
  if (!splash) return -1;
  return 0;
}

/* Check whether all monsters are dead and remove DEATHGATE cells if so.
 */

static int ps_game_remove_deathgate(struct ps_game *game) {
  if (!game) return -1;
  if (!game->grid) return -1;
  int removec=0;
  int i=game->grid->poic;
  const struct ps_blueprint_poi *poi=game->grid->poiv;
  for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_DEATHGATE) {
      struct ps_grid_cell *cell=game->grid->cellv+poi->y*PS_GRID_COLC+poi->x;
      cell->physics=PS_BLUEPRINT_CELL_VACANT;
      cell->tileid=poi->argv[1];
      removec++;
    }
  }
  if (removec) {
    PS_SFX_DEATHGATE
  }
  return 0;
}

int ps_game_check_deathgate(struct ps_game *game) {
  if (!game) return -1;
  if (!ps_game_monsters_present(game)) {
    if (ps_game_remove_deathgate(game)<0) return -1;
  }
  return 0;
}

/* Heal all heroes.
 */
 
int ps_game_heal_all_heroes(struct ps_game *game) {
  if (!game) return -1;
  struct ps_sprgrp *grp=game->grpv+PS_SPRGRP_HERO;
  int i=grp->sprc; while (i-->0) {
    struct ps_sprite *hero=grp->sprv[i];
    if (hero->type==&ps_sprtype_hero) {
      if (ps_hero_become_living(game,hero)<0) return -1;
    }
  }
  return 0;
}

/* Non-persistent grid change.
 */

static int ps_game_npgc_require(struct ps_game *game) {
  if (game->npgcc<game->npgca) return 0;
  int na=game->npgca+16;
  if (na>INT_MAX/sizeof(struct ps_game_npgc)) return -1;
  void *nv=realloc(game->npgcv,sizeof(struct ps_game_npgc)*na);
  if (!nv) return -1;
  game->npgcv=nv;
  game->npgca=na;
  return 0;
}
 
int ps_game_apply_nonpersistent_grid_change(struct ps_game *game,int col,int row,int tileid,int physics,int shape) {
  if (!game) return -1;
  if (!game->grid) return -1;
  if ((col<0)||(col>=PS_GRID_COLC)) return -1;
  if ((row<0)||(row>=PS_GRID_ROWC)) return -1;

  /* Acquire the cell and terminate if redundant. */
  struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
  if ((tileid<0)||(tileid>=256)) tileid=cell->tileid;
  if ((physics<0)||(physics>=256)) physics=cell->physics;
  if ((shape<0)||(shape>=256)) shape=cell->shape;
  if ((tileid==cell->tileid)&&(physics==cell->physics)&&(shape==cell->shape)) return 0;

  /* Add the restoration record. */
  if (ps_game_npgc_require(game)<0) return -1;
  struct ps_game_npgc *npgc=game->npgcv+game->npgcc++;
  npgc->col=col;
  npgc->row=row;
  npgc->tileid=cell->tileid;
  npgc->physics=cell->physics;
  npgc->shape=cell->shape;

  /* Finally, modify the live cell. */
  cell->tileid=tileid;
  cell->physics=physics;
  cell->shape=shape;
  
  return 0;
}

static int ps_game_npgc_pop(struct ps_game *game) {
  if (!game||!game->grid) return -1;
  while (game->npgcc>0) {
    struct ps_game_npgc *npgc=game->npgcv+(--(game->npgcc));
    struct ps_grid_cell *cell=game->grid->cellv+npgc->row*PS_GRID_COLC+npgc->col;
    cell->tileid=npgc->tileid;
    cell->physics=npgc->physics;
    cell->shape=npgc->shape;
  }
  return 0;
}

int ps_game_reverse_nonpersistent_grid_change(struct ps_game *game,int col,int row) {
  if (!game||!game->grid) return -1;
  if ((col<0)||(col>=PS_GRID_COLC)) return 0;
  if ((row<0)||(row>=PS_GRID_ROWC)) return 0;

  /* We only care about the last npgc on this cell, and it's OK if there's none. */
  struct ps_game_npgc npgc;
  int found=0;
  int i=game->npgcc; while (i-->0) {
    struct ps_game_npgc *ck=game->npgcv+i;
    if ((ck->col==col)&&(ck->row==row)) {
      memcpy(&npgc,ck,sizeof(struct ps_game_npgc));
      found=1;
      game->npgcc--;
      memmove(game->npgcv+i,game->npgcv+i+1,sizeof(struct ps_game_npgc)*(game->npgcc-i));
      break;
    }
  }
  if (!found) return 0;

  struct ps_grid_cell *cell=game->grid->cellv+row*PS_GRID_COLC+col;
  cell->tileid=npgc.tileid;
  cell->physics=npgc.physics;
  cell->shape=npgc.shape;

  return 0;
}
