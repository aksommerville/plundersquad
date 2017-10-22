#ifndef PS_GUI_INTERNAL_H
#define PS_GUI_INTERNAL_H

#include "ps.h"
#include "ps_gui.h"
#include "ps_page.h"
#include "ps_widget.h"
#include "video/ps_video_layer.h"

struct ps_gui {
  struct ps_video_layer *layer;
  struct ps_page *page;
};

struct ps_gui_layer {
  struct ps_video_layer hdr;
  struct ps_gui *gui; // WEAK reference to parent.
};

#define LAYER ((struct ps_gui_layer*)(gui->layer))

#endif
