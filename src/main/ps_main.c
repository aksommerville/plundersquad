#include "ps.h"
#include "os/ps_ioc.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"
#include "game/ps_game.h"
#include "gui/ps_gui.h"
#include <time.h>

#if PS_USE_macioc
  #include "opt/macioc/ps_macioc.h"
#endif
#if PS_USE_macwm
  #include "opt/macwm/ps_macwm.h"
#endif
#if PS_USE_machid
  #include "opt/machid/ps_machid.h"
#endif

/* Globals.
 */

static struct ps_game *ps_game=0;
static struct ps_gui *ps_gui=0;

/* Setup for quick testing during development.
 */

static int ps_setup_test_game(int playerc,int difficulty,int length,int test_scgen) {
  int i;
  
  if (ps_game_set_player_count(ps_game,playerc)<0) return -1;
  for (i=1;i<=playerc;i++) {
    if (ps_game_set_player_definition(ps_game,i,i)<0) return -1;
  }

  /* Optionally override plrdef selection. */
  if (ps_game_set_player_definition(ps_game,1,2)<0) return -1;
  
  if (ps_game_set_difficulty(ps_game,difficulty)<0) return -1;
  if (ps_game_set_length(ps_game,length)<0) return -1;

  if (test_scgen) {
    if (ps_game_generate_test(ps_game,
      1, // regionid
      // blueprintids. You must provide at least one with a HERO POI.
      1
      //2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19
    )<0) return -1;
  } else {
    if (ps_game_generate(ps_game)<0) return -1;
  }

  if (ps_game_restart(ps_game)<0) return -1;
  return 0;
}

/* Init.
 */

static int ps_main_init() {
  ps_log(MAIN,TRACE,"%s",__func__);

  int randseed=time(0);
  ps_log(MAIN,INFO,"Random seed %d.",randseed);
  srand(randseed);

  if (ps_video_init()<0) return -1;

  if (ps_input_init()<0) return -1;
  #if PS_USE_macioc
    if (ps_macioc_connect_input()<0) return -1;
  #endif
  #if PS_USE_macwm
    if (ps_macwm_connect_input()<0) return -1;
  #endif
  #if PS_USE_machid
    {
      if (ps_machid_init(&ps_machid_default_delegate)<0) return -1;
      struct ps_input_provider *provider=ps_machid_new_provider();
      if (ps_input_install_provider(provider)<0) return -1;
    }
  #endif
  if (ps_input_load_configuration("etc/input.cfg")<0) return -1; //TODO input config path

  if (ps_resmgr_init("src/data",0)<0) return -1; //TODO resource path

  if (!(ps_game=ps_game_new())) return -1;

  if (!(ps_gui=ps_gui_new())) return -1;
  if (ps_gui_set_game(ps_gui,ps_game)<0) return -1;

  if (1) { // Nonzero for normal interactive setup, zero for quick testing setup
    if (ps_gui_load_page_assemble(ps_gui)<0) return -1;
  } else {
    if (ps_setup_test_game(
      1, // playerc: 1..8
      9, // difficulty: 1..9
      1, // length: 1..9
      1  // Nonzero for fake scenario (configure above).
    )<0) return -1;
  }
  
  return 0;
}

/* Quit.
 */

static void ps_main_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);

  ps_gui_del(ps_gui);
  ps_game_del(ps_game);

  ps_resmgr_quit();
  ps_input_quit();
  ps_video_quit();

  #if PS_USE_machid
    ps_machid_destroy_global_provider();
    ps_machid_quit();
  #endif
  
}

/* Update.
 */

static int ps_main_update() {
  if (ps_input_update()<0) return -1;

  if (ps_input_termination_requested()) {
    ps_ioc_quit(0);
  }

  if (ps_gui_is_active(ps_gui)) {
    if (ps_gui_update(ps_gui)<0) return -1;
  } else if (ps_game) {
    if (ps_game->paused) {
      if (ps_gui_load_page_pause(ps_gui)<0) return -1;
    } else {
      if (ps_game_update(ps_game)<0) return -1;
      if (ps_game->finished) {
        if (ps_gui_load_page_gameover(ps_gui)<0) return -1;
      }
    }
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
