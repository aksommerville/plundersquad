/* ps_widget_assemblepage.c TODO
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "../corewidgets/ps_corewidgets.h"

/* Object definition.
 */

struct ps_widget_assemblepage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_assemblepage*)widget)

/* Delete.
 */

static void _ps_assemblepage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_assemblepage_init(struct ps_widget *widget) {

  widget->bgrgba=0xff0000ff;

  return 0;
}

/* General properties.
 */

static int _ps_assemblepage_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_assemblepage_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_assemblepage_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int _ps_assemblepage_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_assemblepage_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_assemblepage_pack(struct ps_widget *widget) {
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    // Set child bounds.
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_assemblepage_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_assemblepage_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_assemblepage_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_assemblepage_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_assemblepage_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_assemblepage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_assemblepage_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_assemblepage_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_assemblepage_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_assemblepage_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_assemblepage_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_assemblepage_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_assemblepage_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_assemblepage={

  .name="assemblepage",
  .objlen=sizeof(struct ps_widget_assemblepage),

  .del=_ps_assemblepage_del,
  .init=_ps_assemblepage_init,

  .get_property=_ps_assemblepage_get_property,
  .set_property=_ps_assemblepage_set_property,
  .get_property_type=_ps_assemblepage_get_property_type,

  .draw=_ps_assemblepage_draw,
  .measure=_ps_assemblepage_measure,
  .pack=_ps_assemblepage_pack,

  .update=_ps_assemblepage_update,
  .mousemotion=_ps_assemblepage_mousemotion,
  .mousebutton=_ps_assemblepage_mousebutton,
  .mousewheel=_ps_assemblepage_mousewheel,
  .key=_ps_assemblepage_key,
  .userinput=_ps_assemblepage_userinput,

  .mouseenter=_ps_assemblepage_mouseenter,
  .mouseexit=_ps_assemblepage_mouseexit,
  .activate=_ps_assemblepage_activate,
  .cancel=_ps_assemblepage_cancel,
  .adjust=_ps_assemblepage_adjust,
  .focus=_ps_assemblepage_focus,
  .unfocus=_ps_assemblepage_unfocus,

};
