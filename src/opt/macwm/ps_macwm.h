/* ps_macwm.h
 * Bare-bones window manager interface for MacOS.
 * Link: -framework Cocoa -framework OpenGL
 */

#ifndef PS_MACWM_H
#define PS_MACWM_H

int ps_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title
);

void ps_macwm_quit();

int ps_macwm_show_cursor(int show);

int ps_macwm_flush_video();

#endif
