#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_fs.h"
#include "os/ps_emergency_abort.h"
#include "os/ps_userconfig.h"
#include "util/ps_perfmon.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "input/ps_input_provider_mock.h"
#include "input/ps_input_provider.h"
#include "res/ps_resmgr.h"
#include "game/ps_game.h"
#include "gui/ps_gui.h"
#include "gui/ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"
#include <time.h>
#include "ps_developer_setup.h"

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
#if PS_USE_glx
  #include "opt/glx/ps_glx.h"
#endif
#if PS_USE_mswm
  #include "opt/mswm/ps_mswm.h"
#endif
#if PS_USE_mshid
  #include "opt/mshid/ps_mshid.h"
#endif

/* Globals.
 */

static struct ps_game *ps_game=0;
static struct ps_gui *ps_gui=0;
static struct ps_perfmon *ps_perfmon=0;

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

static int ps_main_init_input(struct ps_userconfig *userconfig) {

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
  #if PS_USE_glx
    if (ps_glx_connect_input()<0) return -1;
  #endif
  #if PS_USE_mswm
    if (ps_mswm_connect_input()<0) return -1;
  #endif
  #if PS_USE_mshid
    if (ps_mshid_init()<0) return -1;
  #endif
  
  /* XXX TEMP: Create a mock input provider for testing. */
  { struct ps_input_provider *provider=ps_input_provider_mock_new();
    if (!provider) return -1;
    if (ps_input_install_provider(provider)<0) return -1;
    ps_input_provider_del(provider);
    
    if (ps_input_provider_mock_add_device(provider,"FakeOne")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeTwo")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeThree")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeFour")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeFive")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeSix")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeSeven")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeEight")<0) return -1;
    if (ps_input_provider_mock_add_device(provider,"FakeNine")<0) return -1;
  }

  /* Load configuration and take it live. */
  const char *input_config_path=0;
  if (ps_userconfig_peek_field_as_string(&input_config_path,userconfig,ps_userconfig_search_field(userconfig,"input",5))<0) return -1;
  if (ps_input_load_configuration(input_config_path)<0) return -1;
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

static int ps_main_init_audio(struct ps_userconfig *userconfig) {

  #define PS_AKAU_ENABLE 1
  
  #if PS_USE_akmacaudio
    if (akau_init(&akau_driver_akmacaudio,ps_log_akau)<0) return -1;
  #elif PS_USE_alsa
    if (akau_init(&akau_driver_alsa,ps_log_akau)<0) return -1;
  #elif PS_USE_msaudio
    if (akau_init(&akau_driver_msaudio,ps_log_akau)<0) return -1;
  #else
    #undef PS_AKAU_ENABLE
    #define PS_AKAU_ENABLE 0
    ps_log(MAIN,WARNING,"No audio provider was configured. All audio is disabled.");
    return 0;
  #endif

  #if PS_AKAU_ENABLE
    int bgm_level=ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"music",5));
    int sfx_level=ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"sound",5));
    if (akau_set_trim_for_intent(AKAU_INTENT_BGM,bgm_level)<0) return -1;
    if (akau_set_trim_for_intent(AKAU_INTENT_SFX,sfx_level)<0) return -1;
  #endif
  
  return 0;
}

/* Init.
 */

static int ps_main_init(struct ps_userconfig *userconfig) {
  ps_log(MAIN,TRACE,"%s",__func__);

  ps_perfmon=ps_perfmon_new();
  ps_perfmon_set_autolog(ps_perfmon,5000000);

  if (PS_B_TO_SWAP_INPUT) {
    ps_log(MAIN,WARNING,"Press B to swap input -- Do not leave this enabled in production builds!");
  }

  int randseed=time(0);
  ps_log(MAIN,INFO,"Random seed %d.",randseed);
  srand(randseed);

  if (ps_video_init(userconfig)<0) return -1;

  if (ps_main_init_input(userconfig)<0) {
    ps_log(MAIN,ERROR,"Failed to initialize input.");
    return -1;
  }

  if (ps_main_init_audio(userconfig)<0) {
    ps_log(MAIN,ERROR,"Failed to initialize audio.");
    return -1;
  }

  const char *resources_path=0;
  if (ps_userconfig_peek_field_as_string(&resources_path,userconfig,ps_userconfig_search_field(userconfig,"resources",9))<0) return -1;
  if (ps_resmgr_init(resources_path,0)<0) return -1;

  if (!(ps_game=ps_game_new(userconfig))) return -1;

  if (!(ps_gui=ps_gui_new())) return -1;
  if (ps_gui_set_game(ps_gui,ps_game)<0) return -1;
  if (ps_input_set_gui(ps_gui)<0) return -1;
  if (ps_gui_set_userconfig(ps_gui,userconfig)<0) return -1;

  //if (cmdline->saved_game_path) { //TODO saved game
  //  if (ps_setup_restore_game(cmdline->saved_game_path)<0) return -1;
  //} else {
    int err=ps_setup_test_game(ps_game,userconfig);
    if (err<0) return -1;
    if (!err) {
      if (ps_gui_load_page_assemble(ps_gui)<0) return -1;
    }
  //}

  ps_perfmon_finish_load(ps_perfmon);
  
  return 0;
}

/* Quit.
 */

static void ps_main_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);
  ps_perfmon_begin_quit(ps_perfmon);
  ps_emergency_abort_set(3000000);

  #if PS_AKAU_ENABLE
    ps_emergency_abort_set_message("Shutting down audio.");
    akau_quit();
  #endif
  
  ps_emergency_abort_set_message("Shutting down high-level controllers.");
  ps_gui_del(ps_gui);
  ps_drop_global_gui();
  ps_game_del(ps_game);
  
  ps_emergency_abort_set_message("Shutting down resource manager.");
  ps_resmgr_quit();
  
  ps_emergency_abort_set_message("Shutting down input.");
  ps_input_quit();
  
  ps_emergency_abort_set_message("Shutting down video.");
  ps_video_quit();
  
  #if PS_USE_machid
    ps_emergency_abort_set_message("Shutting down Mac HID manager.");
    ps_machid_destroy_global_provider();
    ps_machid_quit();
  #endif

  #if PS_USE_evdev
    ps_emergency_abort_set_message("Shutting down Linux input (evdev).");
    ps_evdev_quit();
  #endif

  #if PS_USE_mshid
    ps_emergency_abort_set_message("Shutting down Windows input manager.");
    ps_mshid_quit();
  #endif

  ps_perfmon_del(ps_perfmon);
  ps_perfmon=0;
  ps_emergency_abort_set_message("Application shutdown complete.");
}

/* Update.
 */

static int ps_main_update() {
  //ps_log(MAIN,TRACE,"%s",__func__);

  ps_perfmon_update(ps_perfmon);
  
  if (ps_input_update()<0) return -1;

  if (ps_input_termination_requested()) {
    ps_ioc_quit(0);
  }

  #if PS_AKAU_ENABLE
    if (akau_update()<0) return -1;
  #endif

  if (ps_gui_is_active(ps_gui)) {
    if (ps_gui_update(ps_gui)<0) return -1;
    if (ps_userconfig_save_file(ps_gui_get_userconfig(ps_gui))<0) return -1; // It's OK to spam this; it has a dirty flag.
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

  if (ps_video_update()<0) {
    ps_log(MAIN,ERROR,"Error rendering.");
    return -1;
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
