#ifndef PS_GUI_INTERNAL_H
#define PS_GUI_INTERNAL_H

#include "ps.h"
#include "ps_gui.h"
#include "ps_animation.h"
#include "ps_widget.h"
#include "video/ps_video_layer.h"
#include "os/ps_userconfig.h"

extern struct ps_gui *ps_gui_global;

struct ps_gui {
  int refc;
  struct ps_game *game;
  struct ps_widget *root;
  struct ps_animation **animationv;
  int animationc,animationa;
  struct ps_video_layer *layer;
  struct ps_userconfig *userconfig;
};

struct ps_gui_layer {
  struct ps_video_layer hdr;
  struct ps_gui *gui; // WEAK reference to parent.
};

#define LAYER ((struct ps_gui_layer*)(gui->layer))

#endif
