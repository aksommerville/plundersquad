#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_userconfig.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "res/ps_resmgr.h"
#include "gui/ps_gui.h"
#include "gui/ps_widget.h"
#include "gui/editor/ps_editor.h"
#include "akau/akau.h"

#if PS_USE_macioc
  #include "opt/macioc/ps_macioc.h"
#endif
#if PS_USE_macwm
  #include "opt/macwm/ps_macwm.h"
#endif
#if PS_USE_machid
  #include "opt/machid/ps_machid.h"
#endif
#if PS_USE_glx
  #include "opt/glx/ps_glx.h"
#endif
#if PS_USE_mswm
  #include "opt/mswm/ps_mswm.h"
#endif

static struct ps_gui *ps_gui=0;

/* Moved this from ps_gui_object.c so we can eliminate gui/editor from the main executable.
 */

int ps_gui_load_page_edithome(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(ps_gui_get_root(gui),&ps_widget_type_edithome);
  if (!page) return -1;
  if (ps_widget_pack(ps_gui_get_root(gui))<0) return -1;
  return 0;
}

/* Logging from akau.
 */

static void ps_log_akau(int level,const char *msg,int msgc) {
  ps_log(AUDIO,INFO,"[%s] %.*s",akau_loglevel_repr(level),msgc,msg);
}

/* Init.
 */

static int ps_edit_init(struct ps_userconfig *userconfig) {
  ps_log(MAIN,TRACE,"%s",__func__);

  if (ps_video_init(userconfig)<0) return -1;
  
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
  #if PS_USE_glx
    if (ps_glx_connect_input()<0) return -1;
    if (ps_glx_show_cursor(1)<0) return -1;
  #endif
  #if PS_USE_mswm
    if (ps_mswm_connect_input()<0) return -1;
    if (ps_mswm_show_cursor(1)<0) return -1;
  #endif
  if (ps_input_load_configuration("etc/input.cfg")<0) return -1;

  #if PS_USE_akmacaudio
    if (akau_init(&akau_driver_akmacaudio,ps_log_akau)<0) return -1;
  #elif PS_USE_alsa
    if (akau_init(&akau_driver_alsa,ps_log_akau)<0) return -1;
  #elif PS_USE_msaudio
    if (akau_init(&akau_driver_msaudio,ps_log_akau)<0) return -1;
  #endif
  
  if (ps_resmgr_init("src/data",1)<0) return -1;
  
  if (!(ps_gui=ps_gui_new())) return -1;
  if (ps_input_set_gui(ps_gui)<0) return -1;
  if (ps_gui_load_page_edithome(ps_gui)<0) return -1;
  
  return 0;
}

/* Quit.
 */

static void ps_edit_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);

  akau_quit();

  ps_gui_del(ps_gui);
  ps_drop_global_gui();

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

static int ps_edit_update() {

  if (ps_input_update()<0) {
    ps_log(EDIT,ERROR,"ps_input_update failed");
    return -1;
  }
  if (ps_input_termination_requested()) {
    ps_ioc_quit(0);
  }
  
  if (akau_update()<0) {
    ps_log(EDIT,ERROR,"akau_update failed");
    return -1;
  }
  
  if (ps_gui_update(ps_gui)<0) {
    ps_log(EDIT,ERROR,"ps_gui_update failed");
    return -1;
  }
  
  if (ps_video_update()<0) {
    ps_log(EDIT,ERROR,"ps_video_update failed");
    return -1;
  }
  
  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct ps_ioc_delegate delegate={
    .init=ps_edit_init,
    .quit=ps_edit_quit,
    .update=ps_edit_update,
  };
  return ps_ioc_main(argc,argv,&delegate);
}
