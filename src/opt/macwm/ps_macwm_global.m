#include "ps_macwm_internal.h"

struct ps_macwm ps_macwm={0};

/* Init.
 */

int ps_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title
) {
  if (ps_macwm.window) {
    return -1;
  }
  memset(&ps_macwm,0,sizeof(struct ps_macwm));

  if (!(ps_macwm.window=[PsWindow newWithWidth:w height:h title:title fullscreen:fullscreen])) {
    return -1;
  }

  return 0;
}

/* Connect input.
 */

int ps_macwm_connect_input() {
  if (!ps_macwm.window) return -1;

  if (ps_macwm.input_provider) return 0;
  if (!(ps_macwm.input_provider=ps_input_provider_new(0))) return -1;
  ps_macwm.input_provider->providerid=PS_INPUT_PROVIDER_macwm;
  if (ps_input_install_provider(ps_macwm.input_provider)<0) return -1;

  if (!(ps_macwm.device_wm=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_macwm.device_wm,"Window Manager",-1)<0) return -1;
  ps_macwm.device_wm->report_buttons=ps_macwm_report_buttons_wm;
  if (ps_input_provider_install_device(ps_macwm.input_provider,ps_macwm.device_wm)<0) return -1;
  if (ps_input_event_connect(ps_macwm.device_wm)<0) return -1;

  if (!(ps_macwm.device_keyboard=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_macwm.device_keyboard,"Keyboard",-1)<0) return -1;
  ps_macwm.device_keyboard->report_buttons=ps_macwm_report_buttons_keyboard;
  if (ps_input_provider_install_device(ps_macwm.input_provider,ps_macwm.device_keyboard)<0) return -1;
  if (ps_input_event_connect(ps_macwm.device_keyboard)<0) return -1;

  if (!(ps_macwm.device_mouse=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_macwm.device_mouse,"Mouse",-1)<0) return -1;
  ps_macwm.device_mouse->report_buttons=ps_macwm_report_buttons_mouse;
  if (ps_input_provider_install_device(ps_macwm.input_provider,ps_macwm.device_mouse)<0) return -1;
  if (ps_input_event_connect(ps_macwm.device_mouse)<0) return -1;
  
  return 0;
}

/* Quit.
 */

void ps_macwm_quit() {
  [ps_macwm.window release];

  ps_input_device_del(ps_macwm.device_wm);
  ps_input_device_del(ps_macwm.device_keyboard);
  ps_input_device_del(ps_macwm.device_mouse);
  ps_input_uninstall_provider(ps_macwm.input_provider);
  ps_input_provider_del(ps_macwm.input_provider);
  
  memset(&ps_macwm,0,sizeof(struct ps_macwm));
}

/* Abort.
 */
 
void ps_macwm_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    ps_log(MACWM,FATAL,"%.*s",msgc,msg);
  }
  [NSApplication.sharedApplication terminate:nil];
}

/* Show or hide cursor.
 */

int ps_macwm_show_cursor(int show) {
  if (!ps_macwm.window) return -1;
  if (show) {
    if (ps_macwm.window->cursor_visible) return 0;
    [NSCursor unhide];
    ps_macwm.window->cursor_visible=1;
  } else {
    if (!ps_macwm.window->cursor_visible) return 0;
    [NSCursor hide];
    ps_macwm.window->cursor_visible=0;
  }
  return 0;
}

/* Toggle fullscreen.
 */

int ps_macwm_toggle_fullscreen() {

  [ps_macwm.window toggleFullScreen:ps_macwm.window];

  // Take it on faith that the state will change:
  return ps_macwm.window->fullscreen^1;
}

/* OpenGL frame control.
 */

int ps_macwm_flush_video() {
  if (!ps_macwm.window) return -1;
  return [ps_macwm.window endFrame];
}

/* Ridiculous hack to ensure key-up events.
 * Unfortunately, during a fullscreen transition we do not receive keyUp events.
 * If the main input is a keyboard, and the user strikes a key to toggle fullscreen,
 * odds are very strong that they will release that key during the transition.
 * We record every key currently held, and forcibly release them after on a fullscreen transition.
 */
 
int ps_macwm_record_key_down(int key) {
  int p=-1;
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]==key) return 0;
    if (!ps_macwm.keys_down[i]) p=i;
  }
  if (p>=0) {
    ps_macwm.keys_down[p]=key;
  }
  return 0;
}

int ps_macwm_release_key_down(int key) {
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]==key) {
      ps_macwm.keys_down[i]=0;
    }
  }
  return 0;
}

int ps_macwm_drop_all_keys() {
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]) {
      int key=ps_macwm_translate_keysym(ps_macwm.keys_down[i]);
      if (key) {
        if (ps_input_event_button(ps_macwm.device_keyboard,key,0)<0) return -1;
        if (ps_input_event_key(key,0,0)<0) return -1;
      }
      ps_macwm.keys_down[i]=0;
    }
  }
  return 0;
}
