#ifndef PS_MSWM_INTERNAL_H
#define PS_MSWM_INTERNAL_H

#include "ps.h"
#include "ps_mswm.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include <windows.h>

#define GLEW_STATIC 1
#include <GL/glew.h>

#define PS_MSWM_WINDOW_CLASS_NAME "com_aksommerville_plundersquad"

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
  struct ps_input_provider *input_provider;
} ps_mswm;

LRESULT ps_mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
int ps_mswm_update(struct ps_input_provider *dummy);
int ps_mswm_setup_window();

#endif
