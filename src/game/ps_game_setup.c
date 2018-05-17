#include "ps.h"
#include "ps_game.h"
#include "ps_player.h"
#include "ps_plrdef.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_scgen.h"
#include "input/ps_input.h"
#include "res/ps_resmgr.h"

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
  game->gridx=-1;
  game->gridy=-1;

  ps_scgen_del(scgen);
  return 0;
}

/* Generate test scenario.
 */
 
int _ps_game_generate_test(struct ps_game *game,int regionid,int blueprintid,...) {
  if (!game) return -1;
  if (!game->difficulty) game->difficulty=4;
  if (!game->length) game->length=4;
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

  if (regionid>=0) {
    if (ps_scgen_test_require_region(scgen,regionid)<0) {
      ps_scgen_del(scgen);
      return -1;
    }
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

/* Generate test scenario with all blueprints.
 */
 
int ps_game_generate_all_blueprints_test(struct ps_game *game) {
  if (!game) return -1;
  if (!game->difficulty) game->difficulty=4;
  if (!game->length) game->length=4;
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

  if (ps_scgen_test_generate_all_blueprints(scgen)<0) {
    ps_log(GENERATOR,ERROR,"Failed to generate scenario: %.*s",scgen->msgc,scgen->msg);
    ps_scgen_del(scgen);
    return -1;
  }

  game->scenario=scgen->scenario;
  scgen->scenario=0;

  ps_scgen_del(scgen);
  return 0;
}
