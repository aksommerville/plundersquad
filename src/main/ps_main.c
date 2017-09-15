#include "ps.h"
#include "os/ps_ioc.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"
#include "game/ps_game.h"
#include <time.h>

/* Globals.
 */

static struct ps_game *ps_game=0;

/* Init.
 */

static int ps_main_init() {
  ps_log(MAIN,TRACE,"%s",__func__);

  int randseed=time(0);
  ps_log(MAIN,INFO,"Random seed %d.",randseed);
  srand(randseed);

  if (ps_video_init()<0) return -1;
  if (ps_input_init()<0) return -1;
  if (ps_input_set_default_keyboard_mapping()<0) return -1;
  if (ps_resmgr_init("src/data",0)<0) return -1; //TODO resource path

  if (!(ps_game=ps_game_new())) return -1;

  // TODO The process of game initialization will happen interactively, via GUI.
  switch (1) { // <-- very temporary. Set desired player count here.
    case 1: {
        if (ps_game_set_player_count(ps_game,1)<0) return -1;
        if (ps_game_set_player_skills(ps_game,1,PS_SKILL_HOOKSHOT|PS_SKILL_COMBAT)<0) return -1;
      } break;
    case 2: {
        if (ps_game_set_player_count(ps_game,2)<0) return -1;
        if (ps_game_set_player_skills(ps_game,1,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,2,PS_SKILL_HOOKSHOT|PS_SKILL_COMBAT)<0) return -1;
      } break;
    case 3: {
        if (ps_game_set_player_count(ps_game,3)<0) return -1;
        if (ps_game_set_player_skills(ps_game,1,PS_SKILL_HOOKSHOT|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,2,PS_SKILL_SWORD|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,3,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
      } break;
    case 8: {
        if (ps_game_set_player_count(ps_game,8)<0) return -1;
        if (ps_game_set_player_skills(ps_game,1,PS_SKILL_HOOKSHOT|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,2,PS_SKILL_SWORD|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,3,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,4,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,5,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,6,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,7,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
        if (ps_game_set_player_skills(ps_game,8,PS_SKILL_ARROW|PS_SKILL_COMBAT)<0) return -1;
      } break;
    default: return -1;
  }
  if (ps_game_set_difficulty(ps_game,9)<0) return -1;
  if (ps_game_set_length(ps_game,4)<0) return -1;
  //if (ps_game_generate_test(ps_game,1,1,11,9)<0) return -1;
  if (ps_game_generate(ps_game)<0) return -1;
  if (ps_game_restart(ps_game)<0) return -1;
  
  return 0;
}

/* Quit.
 */

static void ps_main_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);

  ps_game_del(ps_game);

  ps_resmgr_quit();
  ps_input_quit();
  ps_video_quit();
  
}

/* Update.
 */

static int ps_main_update() {
  if (ps_input_update()<0) return -1;

  //TODO Insert GUI here.
  if (ps_game) {
    if (ps_game_update(ps_game)<0) return -1;
  }
  
  if (ps_video_update()<0) return -1;
  return 0;
}

/* Respond to actions from the input core.
 */

int ps_main_input_action_callback(int actionid) {
  switch (actionid) {
    case PS_ACTION_WARPW: return ps_game_force_next_screen(ps_game,-1,0);
    case PS_ACTION_WARPE: return ps_game_force_next_screen(ps_game,1,0);
    case PS_ACTION_WARPN: return ps_game_force_next_screen(ps_game,0,-1);
    case PS_ACTION_WARPS: return ps_game_force_next_screen(ps_game,0,1);
  }
  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct ps_ioc_delegate delegate={
    .init=ps_main_init,
    .quit=ps_main_quit,
    .update=ps_main_update,
  };
  return ps_ioc_main(argc,argv,&delegate);
}
