#ifndef PS_GLX_INTERNAL_H
#define PS_GLX_INTERNAL_H

#include "ps.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glx.h>

#define KeyRepeat (LASTEvent+2)
#define PS_GLX_KEY_REPEAT_INTERVAL 10

extern struct ps_glx {
  
  Display *dpy;
  int screen;
  Window win;
  GLXContext ctx;
  int glx_version_minor,glx_version_major;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  
  int w,h;
  int fullscreen;

  struct ps_input_provider *input_provider;
  struct ps_input_device *dev_keyboard;
  struct ps_input_device *dev_mouse;
  
} ps_glx;

int ps_glx_update(struct ps_input_provider *provider);
int ps_glx_btnid_repr(char *dst,int dsta,int btnid);
int ps_glx_btnid_eval(int *btnid,const char *src,int srcc);
int ps_glx_report_buttons_keyboard(struct ps_input_device *device,void *userdata,int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata));
int ps_glx_report_buttons_mouse(struct ps_input_device *device,void *userdata,int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata));

#endif
