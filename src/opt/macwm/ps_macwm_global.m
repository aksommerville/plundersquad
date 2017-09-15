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

/* Quit.
 */

void ps_macwm_quit() {
  [ps_macwm.window release];
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

/* OpenGL frame control.
 */

int ps_macwm_flush_video() {
  if (!ps_macwm.window) return -1;
  return [ps_macwm.window endFrame];
}
