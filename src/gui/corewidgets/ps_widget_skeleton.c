#include "ps.h"
#include "../ps_widget.h"

/* Object definition.
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

/* General properties.
 */

static int _ps_skeleton_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_skeleton_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_skeleton_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int _ps_skeleton_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
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
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    // Set child bounds.
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_skeleton_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_skeleton_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_skeleton_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_skeleton_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_skeleton_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_skeleton_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_skeleton_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_skeleton_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_skeleton_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_skeleton={

  .name="skeleton",
  .objlen=sizeof(struct ps_widget_skeleton),

  .del=_ps_skeleton_del,
  .init=_ps_skeleton_init,

  .get_property=_ps_skeleton_get_property,
  .set_property=_ps_skeleton_set_property,
  .get_property_type=_ps_skeleton_get_property_type,

  .draw=_ps_skeleton_draw,
  .measure=_ps_skeleton_measure,
  .pack=_ps_skeleton_pack,

  .update=_ps_skeleton_update,
  .mousemotion=_ps_skeleton_mousemotion,
  .mousebutton=_ps_skeleton_mousebutton,
  .mousewheel=_ps_skeleton_mousewheel,
  .key=_ps_skeleton_key,
  .userinput=_ps_skeleton_userinput,

  .mouseenter=_ps_skeleton_mouseenter,
  .mouseexit=_ps_skeleton_mouseexit,
  .activate=_ps_skeleton_activate,
  .cancel=_ps_skeleton_cancel,
  .adjust=_ps_skeleton_adjust,
  .focus=_ps_skeleton_focus,
  .unfocus=_ps_skeleton_unfocus,

};
