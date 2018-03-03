#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_fs.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"
#include "game/ps_game.h"
#include "gui/ps_gui.h"
#include "gui/ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"
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
#if PS_USE_evdev
  #include "opt/evdev/ps_evdev.h"
#endif

/* Globals.
 */

static struct ps_game *ps_game=0;
static struct ps_gui *ps_gui=0;

/* Setup for quick testing during development.
 * This is called whenever no saved game was requested.
 * Return 0 to fall back to interactive setup -- production builds should always do that.
 */

static int ps_setup_test_game(const struct ps_cmdline *cmdline) {
  int i;

  //return 0; // Uncomment this line for normal interactive setup.

  /* Configure players. */
  if (ps_game_set_player_count(ps_game,1)<0) return -1;
  for (i=1;i<=PS_PLAYER_LIMIT;i++) {
    ps_game_configure_player(ps_game,i,i,i,0);
  }
  if (ps_input_set_noninteractive_device_assignment()<0) return -1;

  /* Optionally override plrdef selection. (plrid,plrdefid,palette,device) */
  //001-swordsman 002-archer    003-gadgeteer 004-nurse     005-wizard    006-vampire   007-martyr    008-immortal  009-bomber
  if (ps_game_configure_player(ps_game,1,2,0,0)<0) return -1;
  //if (ps_game_configure_player(ps_game,2,2,0,0)<0) return -1;
  //if (ps_game_configure_player(ps_game,3,3,1,0)<0) return -1;
  //if (ps_game_configure_player(ps_game,4,2,2,0)<0) return -1;

  if (0) { // Generate a scenario just like normal launches.
    if (ps_game_set_difficulty(ps_game,9)<0) return -1;
    if (ps_game_set_length(ps_game,2)<0) return -1;
    if (ps_game_generate(ps_game)<0) return -1;

  } else { // Generate a test scenario -- good for blueprint test drives.
    if (ps_game_generate_test(ps_game,
      -1, // regionid, negative means random
      // blueprintid. At least one must have adequate HERO POI:
      2,46
    )<0) return -1;
  }

  if (ps_game_restart(ps_game)<0) return -1;
  
  return 1;
}

/* Restore game from file.
 */

static int ps_setup_restore_game(const char *path) {
  void *serial=0;
  int serialc=ps_file_read(&serial,path);
  if ((serialc<0)||!serial) {
    ps_log(GAME,ERROR,"%s: Failed to read saved game.",path);
    return -1;
  }
  if (ps_game_decode(ps_game,serial,serialc)<0) {
    ps_log(GAME,ERROR,"%s: Failed to decode game.",path);
    free(serial);
    return -1;
  }
  free(serial);
  if (ps_game_return_to_start_screen(ps_game)<0) return -1;
  ps_log(GAME,INFO,"Restored game from '%s'.",path);
  return 0;
}

/* Init input.
 */

static int ps_main_init_input(const struct ps_cmdline *cmdline) {

  /* Initialize generic input core. */
  if (ps_input_init()<0) return -1;

  /* Initialize and connect providers. */
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
  #if PS_USE_evdev
    if (ps_evdev_init_default()<0) return -1;
  #endif

  /* Load configuration and take it live. */
  if (ps_input_load_configuration(cmdline->input_config_path)<0) return -1;
  if (ps_input_update()<0) return -1; // Reassigns input devices and gets the core running.
  
  return 0;
}

/* Logging from akau.
 */

static void ps_log_akau(int level,const char *msg,int msgc) {
  switch (level) {
    case AKAU_LOGLEVEL_DEBUG: ps_log(AUDIO,DEBUG,"%.*s",msgc,msg); break;
    case AKAU_LOGLEVEL_INFO: ps_log(AUDIO,INFO,"%.*s",msgc,msg); break;
    case AKAU_LOGLEVEL_WARN: ps_log(AUDIO,WARNING,"%.*s",msgc,msg); break;
    case AKAU_LOGLEVEL_ERROR: ps_log(AUDIO,ERROR,"%.*s",msgc,msg); break;
    default: ps_log(AUDIO,INFO,"%.*s",msgc,msg); break;
  }
}

/* Init audio.
 */

static int ps_main_init_audio(const struct ps_cmdline *cmdline) {

  char audiorespath[1024];
  int audiorespathc=snprintf(audiorespath,sizeof(audiorespath),"%s/audio",cmdline->resources_path);
  if ((audiorespathc<1)||(audiorespathc>=sizeof(audiorespath))) {
    ps_log(MAIN,ERROR,"Failed to acquire path for audio resources.");
    return -1;
  }

  #define PS_AKAU_ENABLE 1
  
  #if PS_USE_akmacaudio
    if (akau_init(&akau_driver_akmacaudio,ps_log_akau)<0) return -1;
  #elif PS_USE_alsa
    if (akau_init(&akau_driver_alsa,ps_log_akau)<0) return -1;
  #else
    #undef PS_AKAU_ENABLE
    #define PS_AKAU_ENABLE 0
    ps_log(MAIN,WARNING,"No audio provider was configured. All audio is disabled.");
    return 0;
  #endif
  
  if (akau_load_resources(audiorespath)<0) return -1;
  
  return 0;
}

/* Init.
 */

static int ps_main_init(const struct ps_cmdline *cmdline) {
  ps_log(MAIN,TRACE,"%s",__func__);

  if (PS_B_TO_SWAP_INPUT) {
    ps_log(MAIN,WARNING,"Press B to swap input -- Do not leave this enabled in production builds!");
  }

  int randseed=time(0);
  ps_log(MAIN,INFO,"Random seed %d.",randseed);
  srand(randseed);

  if (ps_video_init()<0) return -1;

  if (ps_main_init_input(cmdline)<0) {
    ps_log(MAIN,ERROR,"Failed to initialize input.");
    return -1;
  }

  if (ps_main_init_audio(cmdline)<0) {
    ps_log(MAIN,ERROR,"Failed to initialize audio.");
    return -1;
  }

  if (ps_resmgr_init(cmdline->resources_path,0)<0) return -1;

  if (!(ps_game=ps_game_new())) return -1;

  if (!(ps_gui=ps_gui_new())) return -1;
  if (ps_gui_set_game(ps_gui,ps_game)<0) return -1;
  if (ps_input_set_gui(ps_gui)<0) return -1;

  if (cmdline->saved_game_path) {
    if (ps_setup_restore_game(cmdline->saved_game_path)<0) return -1;
  } else {
    int err=ps_setup_test_game(cmdline);
    if (err<0) return -1;
    if (!err) {
      if (ps_gui_load_page_assemble(ps_gui)<0) return -1;
    }
  }
  
  return 0;
}

/* Quit.
 */

static void ps_main_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);

  #if PS_AKAU_ENABLE
    akau_quit();
  #endif

  ps_gui_del(ps_gui);
  ps_drop_global_gui();
  ps_game_del(ps_game);

  ps_resmgr_quit();
  ps_input_quit();
  ps_video_quit();

  #if PS_USE_machid
    ps_machid_destroy_global_provider();
    ps_machid_quit();
  #endif

  #if PS_USE_evdev
    ps_evdev_quit();
  #endif
  
}

/* Update.
 */

static int ps_main_update() {
  if (ps_input_update()<0) return -1;

  if (ps_input_termination_requested()) {
    ps_ioc_quit(0);
  }

  #if PS_AKAU_ENABLE
    if (akau_update()<0) return -1;
  #endif

  if (ps_gui_is_active(ps_gui)) {
    if (ps_gui_update(ps_gui)<0) return -1;
  } else if (ps_game) {
    if (ps_game->paused) {
      if (ps_gui_load_page_pause(ps_gui)<0) return -1;
    } else if (ps_game->got_treasure) {
      if (ps_gui_load_page_treasure(ps_gui,ps_game->got_treasure)<0) return -1;
      ps_game->got_treasure=0;
    } else if (ps_game->finished) {
      if (ps_gui_load_page_gameover(ps_gui)<0) return -1;
    } else {
      if (ps_game_update(ps_game)<0) return -1;
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
