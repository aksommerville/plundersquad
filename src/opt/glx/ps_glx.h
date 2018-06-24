/* ps_glx.h
 * Adapter for X11/GLX.
 */
 
#ifndef PS_GLX_H
#define PS_GLX_H

int ps_glx_init(int w,int h,int fullscreen,const char *title);
void ps_glx_quit();
int ps_glx_connect_input();

int ps_glx_swap();

int ps_glx_set_icon(const void *rgba,int w,int h);

/* (state) is 1=fullscreen, 0=window, -1=toggle.
 */
int ps_glx_set_fullscreen(int state);

int ps_glx_show_cursor(int show);

/* X11 screensaver is inhibited by activity from the mouse or keyboard.
 * It is not aware of joysticks, so we have to tell it about that by ourselves.
 * This is hugely important, since discerning players will be using joysticks exclusively,
 * and a game as delightfully enjoyable as this one will certainly be played for longer
 * that the users' screen saver timeouts.
 * We discard extra calls until the next ps_glx_swap(), so it is safe to bombard this with events.
 */
int ps_glx_inhibit_screensaver();

#endif
