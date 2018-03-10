/* ps_glx.h
 * Adapter for X11/GLX.
 */
 
#ifndef PS_GLX_H
#define PS_GLX_H

int ps_glx_init(int w,int h,int fullscreen,const char *title);
void ps_glx_quit();
int ps_glx_connect_input();

int ps_glx_swap();

//TODO toggle fullscreen

#endif
