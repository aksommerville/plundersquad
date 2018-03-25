#ifndef PS_MSWM_INTERNAL_H
#define PS_MSWM_INTERNAL_H

#include "ps.h"
#include "ps_mswm.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include <windows.h>

#define GLEW_STATIC 1
#include <GL/glew.h>

#define PS_MSWM_WINDOW_CLASS_NAME "com_aksommerville_plundersquad"

#ifndef WM_MOUSEHWHEEL
  #define WM_MOUSEHWHEEL 0x020e
#endif

extern struct ps_mswm {
  int init;
  int window_setup_complete;
  int translate_events;
  HINSTANCE instance;
  WNDCLASSEX wndclass;
  ATOM wndclass_atom;
  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
  int winw,winh;
  int fullscreen;
  int cursor;
  WINDOWPLACEMENT fsrestore;
  struct ps_input_provider *input_provider;
  struct ps_input_device *dev_keyboard;
} ps_mswm;

LRESULT ps_mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
int ps_mswm_update(struct ps_input_provider *dummy);
int ps_mswm_setup_window();

int ps_mswm_report_buttons_keyboard(
  struct ps_input_device *dev,
  void *userdata,
  int (*cb)(struct ps_input_device *dev,const struct ps_input_btncfg *btncfg,void *userdata)
);

int ps_mswm_usage_from_keysym(int keysym);

#endif
