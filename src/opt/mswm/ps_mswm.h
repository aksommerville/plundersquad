/* ps_mswm.h
 * Window manager and IoC for Microsoft Windows.
 */

#ifndef PS_MSWM_H
#define PS_MSWM_H

int ps_mswm_init(int w,int h,int fullscreen,const char *title);
void ps_mswm_quit();
int ps_mswm_connect_input();
int ps_mswm_swap();

// >0=fullscreen, 0=window, <0=toggle
int ps_mswm_set_fullscreen(int flag);

#endif
