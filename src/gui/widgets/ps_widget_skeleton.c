#include "ps.h"
#include "gui/ps_gui.h"

/* Widget definition.
 */

struct ps_widget_skeleton {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_skeleton*)widget)

/* Delete.
 */

static void _ps_skeleton_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_skeleton_init(struct ps_widget *widget) {
  return 0;
}

/* Draw.
 */

static int _ps_skeleton_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_skeleton_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_skeleton_pack(struct ps_widget *widget) {
  return 0;
}

/* Mouse events.
 */

static int _ps_skeleton_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_skeleton_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_skeleton_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_skeleton_mousemove(struct ps_widget *widget,int x,int y) {
  return 0;
}

/* Unified events.
 */

static int _ps_skeleton_activate(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_skeleton={
  .name="skeleton",
  .objlen=sizeof(struct ps_widget_skeleton),
  .del=_ps_skeleton_del,
  .init=_ps_skeleton_init,
  .draw=_ps_skeleton_draw,
  .measure=_ps_skeleton_measure,
  .pack=_ps_skeleton_pack,
  .mouseenter=_ps_skeleton_mouseenter,
  .mouseexit=_ps_skeleton_mouseexit,
  .mousedown=_ps_skeleton_mousedown,
  .mouseup=_ps_skeleton_mouseup,
  .mousewheel=_ps_skeleton_mousewheel,
  .mousemove=_ps_skeleton_mousemove,
  .activate=_ps_skeleton_activate,
};
