#ifndef PS_GUI_INTERNAL_H
#define PS_GUI_INTERNAL_H

#include "ps.h"
#include "ps_gui.h"
#include "ps_page.h"
#include "ps_widget.h"
#include "video/ps_video_layer.h"

struct ps_transition {
  struct ps_widget *widget; // STRONG, widget being modified.
  int k; // Property to modify.
  int va,vz; // Start and end values.
  int p,c; // Position and duration.
};

struct ps_gui {
  struct ps_video_layer *layer;
  struct ps_page *page;
  uint16_t input;
  int use_unified_input;
  struct ps_transition *transitionv;
  int transitionc,transitiona;
  struct ps_game *game; // WEAK
};

struct ps_gui_layer {
  struct ps_video_layer hdr;
  struct ps_gui *gui; // WEAK reference to parent.
};

#define LAYER ((struct ps_gui_layer*)(gui->layer))

void ps_transition_cleanup(struct ps_transition *transition);
int ps_gui_update_transitions(struct ps_gui *gui);

#endif
