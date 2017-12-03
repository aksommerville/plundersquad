/* ps_widget.h
 * Fundamental unit of GUI.
 */

#ifndef PS_WIDGET_H
#define PS_WIDGET_H

struct ps_widget;

struct ps_widget_type {
  const char *name;
  int objlen;

  int (*init)(struct ps_widget *widget);
  void (*del)(struct ps_widget *widget);

  int (*draw)(struct ps_widget *widget,int x0,int y0);

  int (*measure)(int *w,int *h,struct ps_widget *widget,int maxw,int maxh);
  int (*pack)(struct ps_widget *widget);

  int (*mouseenter)(struct ps_widget *widget);
  int (*mouseexit)(struct ps_widget *widget);
  int (*mousedown)(struct ps_widget *widget,int btnid);
  int (*mouseup)(struct ps_widget *widget,int btnid,int inbounds);
  int (*mousewheel)(struct ps_widget *widget,int dx,int dy);
  int (*mousemove)(struct ps_widget *widget,int x,int y); // (x,y) relative to this widget

  int (*activate)(struct ps_widget *widget);

};

struct ps_widget {
  const struct ps_widget_type *type;
  int refc;
  struct ps_widget *parent; // WEAK
  struct ps_widget **childv;
  int childc,childa;
  int x,y,w,h; // Bounds, relative to parent.
  uint32_t bgrgba,fgrgba;
  int track_mouse; // Must set nonzero to receive mouse events. Children do not require it of parents.
};

struct ps_widget *ps_widget_new(const struct ps_widget_type *type);
void ps_widget_del(struct ps_widget *widget);
int ps_widget_ref(struct ps_widget *widget);

int ps_widget_is_ancestor(const struct ps_widget *ancestor,const struct ps_widget *descendant);
int ps_widget_add_child(struct ps_widget *parent,struct ps_widget *child);
int ps_widget_remove_child(struct ps_widget *parent,struct ps_widget *child);
int ps_widget_remove_all_children(struct ps_widget *widget);

/* Create a widget, add to parent, and return WEAK reference.
 */
struct ps_widget *ps_widget_spawn(struct ps_widget *parent,const struct ps_widget_type *type);

/* Draw a widget, or components of it.
 * If the widget's type implements (draw), which it should, it is responsible for everything.
 * A (draw) implementation typically calls ps_widget_draw_children().
 * Widget positions are relative to parent, so you must provide the parent's position (x0,y0).
 */
int ps_widget_draw(struct ps_widget *widget,int x0,int y0);
int ps_widget_draw_children(struct ps_widget *widget,int x0,int y0);
int ps_widget_draw_background(struct ps_widget *widget,int x0,int y0);

/* Return the preferred size of a widget, given limits on each axis.
 * Parents are responsible for sizing their children, and a widget must accept whatever size it gets.
 */
int ps_widget_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh);

/* Place children and perform any other acceptance of the widget's bounds.
 * Prior to this call, the widget's parent should set its (x,y,w,h).
 * Implementations of (pack) typically recur into all children.
 */
int ps_widget_pack(struct ps_widget *widget);

/* Generic properties, PS_GUI_PROPERTY_*
 * Setting property returns >0 if actually changed.
 */
int ps_widget_set_property(struct ps_widget *widget,int k,int v);
int ps_widget_get_property(const struct ps_widget *widget,int k);

struct ps_gui *ps_widget_get_gui(const struct ps_widget *widget);
struct ps_page *ps_widget_get_page(const struct ps_widget *widget);

int ps_widget_contains_point(const struct ps_widget *widget,int x,int y);

int ps_widget_event_mouseenter(struct ps_widget *widget);
int ps_widget_event_mouseexit(struct ps_widget *widget);
int ps_widget_event_mousedown(struct ps_widget *widget,int btnid);
int ps_widget_event_mouseup(struct ps_widget *widget,int btnid,int inbounds);
int ps_widget_event_mousewheel(struct ps_widget *widget,int dx,int dy);
int ps_widget_event_mousemove(struct ps_widget *widget,int x,int y);

int ps_widget_event_activate(struct ps_widget *widget);

int ps_widget_coords_from_window(int *x,int *y,const struct ps_widget *widget,int winx,int winy);

#endif
