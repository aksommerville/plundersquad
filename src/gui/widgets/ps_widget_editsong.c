#include "ps.h"
#include "gui/ps_gui.h"

/* Widget definition.
 */

struct ps_widget_editsong {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_editsong*)widget)

/* Delete.
 */

static void _ps_editsong_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_editsong_init(struct ps_widget *widget) {
  widget->bgrgba=0x008000ff;
  return 0;
}

/* Draw.
 */

static int _ps_editsong_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_editsong_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_editsong_pack(struct ps_widget *widget) {
  return 0;
}

/* Mouse events.
 */

static int _ps_editsong_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsong_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsong_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_editsong_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_editsong_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_editsong_mousemove(struct ps_widget *widget,int x,int y) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsong={
  .name="editsong",
  .objlen=sizeof(struct ps_widget_editsong),
  .del=_ps_editsong_del,
  .init=_ps_editsong_init,
  .draw=_ps_editsong_draw,
  .measure=_ps_editsong_measure,
  .pack=_ps_editsong_pack,
  .mouseenter=_ps_editsong_mouseenter,
  .mouseexit=_ps_editsong_mouseexit,
  .mousedown=_ps_editsong_mousedown,
  .mouseup=_ps_editsong_mouseup,
  .mousewheel=_ps_editsong_mousewheel,
  .mousemove=_ps_editsong_mousemove,
};

/* Accessors.
 */
 
int ps_widget_editsong_set_song(struct ps_widget *widget,struct akau_song *song) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return -1;
  //TODO
  return 0;
}

struct akau_song *ps_widget_editsong_get_song(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return 0;
  //TODO
  return 0;
}

int ps_widget_editsong_set_path(struct ps_widget *widget,const char *path) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return -1;
  //TODO
  return 0;
}
