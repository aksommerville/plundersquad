// XXX I don't think "input action" is something we actually use. Look into removing this.

#include "ps.h"
#include "ps_input_button.h"

/* Warp. For debug purposes, switch immediately to a neighbor screen.
 */

static int ps_input_action_warp(int dx,int dy) {
  return 0;
}

/* Pause.
 */

static int ps_input_action_pause(int flag) {
  return 0;
}

/* Fullscreen.
 */

static int ps_input_action_fullscreen(int flag) {
  return 0;
}

/* Quit.
 */

static int ps_input_action_quit() {
  return 0;
}

/* Screenshot.
 */

static int ps_input_action_screenshot() {
  return 0;
}

/* Enter in-game debugger.
 */

static int ps_input_action_debug() {
  return 0;
}

/* Fire action, dispatcher.
 */
 
int ps_input_fire_action(int actionid) {
  ps_log(INPUT,TRACE,"%s %s(0x%08x)",__func__,ps_action_repr(actionid),actionid);
  switch (actionid) {
    case PS_ACTION_WARPN: return ps_input_action_warp(0,-1);
    case PS_ACTION_WARPS: return ps_input_action_warp(0,1);
    case PS_ACTION_WARPW: return ps_input_action_warp(-1,0);
    case PS_ACTION_WARPE: return ps_input_action_warp(1,0);
    case PS_ACTION_PAUSE: return ps_input_action_pause(-1);
    case PS_ACTION_PAUSEON: return ps_input_action_pause(1);
    case PS_ACTION_PAUSEOFF: return ps_input_action_pause(0);
    case PS_ACTION_FULLSCREEN: return ps_input_action_fullscreen(-1);
    case PS_ACTION_FULLSCREENON: return ps_input_action_fullscreen(1);
    case PS_ACTION_FULLSCREENOFF: return ps_input_action_fullscreen(0);
    case PS_ACTION_QUIT: return ps_input_action_quit();
    case PS_ACTION_SCREENSHOT: return ps_input_action_screenshot();
    case PS_ACTION_DEBUG: return ps_input_action_debug();
  }
  return 0;
}
