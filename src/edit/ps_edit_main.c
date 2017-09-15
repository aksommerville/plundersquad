#include "ps.h"
#include "os/ps_ioc.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "res/ps_resmgr.h"

/* Init.
 */

static int ps_edit_init() {
  ps_log(MAIN,TRACE,"%s",__func__);

  if (ps_video_init()<0) return -1;
  if (ps_input_init()<0) return -1;
  if (ps_resmgr_init("src/data",0)<0) return -1; //TODO resource path
  
  return 0;
}

/* Quit.
 */

static void ps_edit_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);

  ps_resmgr_quit();
  ps_input_quit();
  ps_video_quit();
  
}

/* Update.
 */

static int ps_edit_update() {
  if (ps_input_update()<0) return -1;
  if (ps_video_update()<0) return -1;
  return 0;
}

/* Respond to input actions.
 */
 
int ps_main_input_action_callback(int actionid) {
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
