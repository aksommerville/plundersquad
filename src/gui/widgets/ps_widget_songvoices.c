#include "ps.h"
#include "gui/ps_gui.h"

/* Widget definition.
 */

struct ps_widget_songvoices {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_songvoices*)widget)

/* Delete.
 */

static void _ps_songvoices_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_songvoices_init(struct ps_widget *widget) {

  widget->bgrgba=0x606060ff;

  return 0;
}

/* Draw.
 */

static int _ps_songvoices_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_songvoices_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_songvoices_pack(struct ps_widget *widget) {
  return 0;
}

/* Mouse events.
 */

static int _ps_songvoices_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_songvoices_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_songvoices_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_songvoices_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_songvoices_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_songvoices_mousemove(struct ps_widget *widget,int x,int y) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_songvoices={
  .name="songvoices",
  .objlen=sizeof(struct ps_widget_songvoices),
  .del=_ps_songvoices_del,
  .init=_ps_songvoices_init,
  .draw=_ps_songvoices_draw,
  .measure=_ps_songvoices_measure,
  .pack=_ps_songvoices_pack,
  .mouseenter=_ps_songvoices_mouseenter,
  .mouseexit=_ps_songvoices_mouseexit,
  .mousedown=_ps_songvoices_mousedown,
  .mouseup=_ps_songvoices_mouseup,
  .mousewheel=_ps_songvoices_mousewheel,
  .mousemove=_ps_songvoices_mousemove,
};
