#ifndef PS_GUI_INTERNAL_H
#define PS_GUI_INTERNAL_H

#include "ps.h"
#include "ps_gui.h"
#include "ps_page.h"
#include "ps_widget.h"
#include "video/ps_video_layer.h"

#define PS_TRANSITION_MODE_ONCE     0
#define PS_TRANSITION_MODE_REPEAT   1

/* Transitions and ongoing animations are all lumped together.
 * At first I thought they should be separate, but it turns out they are basically the same thing.
 */
struct ps_transition {
  struct ps_widget *widget; // STRONG, widget being modified.
  int k; // Property to modify.
  int va,vz; // Start and end values.
  int p,c; // Position and duration.
  int mode; // PS_TRANSITION_MODE_*
};

struct ps_gui {
  int refc;
  struct ps_video_layer *layer;
  struct ps_page *page;
  uint16_t input;
  int use_unified_input;
  struct ps_transition *transitionv;
  int transitionc,transitiona;
  struct ps_game *game; // WEAK

  // Mouse events:
  int mousex,mousey;
  int track_btnid;
  struct ps_widget *track_hover;
  struct ps_widget *track_click;
};

struct ps_gui_layer {
  struct ps_video_layer hdr;
  struct ps_gui *gui; // WEAK reference to parent.
};

#define LAYER ((struct ps_gui_layer*)(gui->layer))

void ps_transition_cleanup(struct ps_transition *transition);
int ps_gui_update_transitions(struct ps_gui *gui);

#endif
