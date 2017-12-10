/* ps_widget.h
 * Generic UI element.
 * All the action of the gui unit is expressed as hierarchical relationships of widgets.
 */

#ifndef PS_WIDGET_H
#define PS_WIDGET_H

struct ps_widget;
struct ps_widget_type;

#define PS_WIDGET_PROPERTY_TYPE_UNDEFINED   0 /* Invalid key. */
#define PS_WIDGET_PROPERTY_TYPE_INTEGER     1 /* Animatable integer. */
#define PS_WIDGET_PROPERTY_TYPE_RGBA        2 /* Animatable as four separate bytes. */
#define PS_WIDGET_PROPERTY_TYPE_ENUM        3 /* Not animatable. */
#define PS_WIDGET_PROPERTY_TYPE_HOTINTEGER  4 /* Animatable integer, must repack on change (ie 'w' and 'h'). */

#define PS_WIDGET_PROPERTY_x       1
#define PS_WIDGET_PROPERTY_y       2
#define PS_WIDGET_PROPERTY_w       3
#define PS_WIDGET_PROPERTY_h       4
#define PS_WIDGET_PROPERTY_bgrgba  5
#define PS_WIDGET_PROPERTY_fgrgba  6

/* Widget type definition.
 */

struct ps_widget_type {
  const char *name;
  int objlen;

  void (*del)(struct ps_widget *widget);
  int (*init)(struct ps_widget *widget);

  int (*get_property)(int *v,const struct ps_widget *widget,int k);
  int (*set_property)(struct ps_widget *widget,int k,int v);
  int (*get_property_type)(const struct ps_widget *widget,int k);

  int (*draw)(struct ps_widget *widget,int parentx,int parenty);

  int (*measure)(int *w,int *h,struct ps_widget *widget,int maxw,int maxh);
  int (*pack)(struct ps_widget *widget);

  int (*update)(struct ps_widget *widget);
  int (*mousemotion)(struct ps_widget *widget,int x,int y);
  int (*mousebutton)(struct ps_widget *widget,int btnid,int value);
  int (*mousewheel)(struct ps_widget *widget,int dx,int dy);
  int (*key)(struct ps_widget *widget,int keycode,int codepoint,int value);
  int (*userinput)(struct ps_widget *widget,int plrid,int btnid,int value);

  int (*mouseenter)(struct ps_widget *widget);
  int (*mouseexit)(struct ps_widget *widget);
  int (*activate)(struct ps_widget *wiget);
  int (*cancel)(struct ps_widget *widget);
  int (*adjust)(struct ps_widget *widget,int d);
  int (*focus)(struct ps_widget *widget);
  int (*unfocus)(struct ps_widget *widget);

};

/* The base widget object.
 */

struct ps_widget {
  const struct ps_widget_type *type;
  int refc;
  
  struct ps_widget *parent; // WEAK
  struct ps_widget **childv;
  int childc,childa;

  int x,y; // Position relative to parent.
  int w,h;
  uint32_t bgrgba,fgrgba;

  int accept_mouse_focus;
  int accept_keyboard_focus;
  int draggable; // Special case of mouse interaction, also managed by root.
  
};

/* Generic widget API.
 */

struct ps_widget *ps_widget_new(const struct ps_widget_type *type);
void ps_widget_del(struct ps_widget *widget);
int ps_widget_ref(struct ps_widget *widget);

/* Manage parent/child relationships.
 */
int ps_widget_is_ancestor(const struct ps_widget *ancestor,const struct ps_widget *descendant);
int ps_widget_has_child(const struct ps_widget *parent,const struct ps_widget *child);
int ps_widget_add_child(struct ps_widget *parent,struct ps_widget *child);
int ps_widget_insert_child(struct ps_widget *parent,int p,struct ps_widget *child);
int ps_widget_remove_child(struct ps_widget *parent,struct ps_widget *child);
int ps_widget_remove_all_children(struct ps_widget *widget);
int ps_widget_kill(struct ps_widget *widget); // Remove from parent and repack parent.

/* Create a new widget and add it as a child of an existing one.
 * This returns a WEAK reference.
 */
struct ps_widget *ps_widget_spawn(struct ps_widget *parent,const struct ps_widget_type *type);

/* Convert between screen space and widget space.
 * Screen space is a constant size (PS_SCREENW,PS_SCREENH).
 * Widget space is relative to the named widget. Its top-left corner is (0,0).
 */
int ps_widget_coords_to_screen(int *screenx,int *screeny,const struct ps_widget *widget,int widgetx,int widgety);
int ps_widget_coords_from_screen(int *widgetx,int *widgety,const struct ps_widget *widget,int screenx,int screeny);
int ps_widget_contains_screen_point(const struct ps_widget *widget,int x,int y);

/* Get the ancestor widget with no parent.
 * For any accessible widget, this is "root" type.
 * It could be some other type if the widget is detached.
 */
struct ps_widget *ps_widget_get_root(const struct ps_widget *widget);
struct ps_gui *ps_widget_get_gui(const struct ps_widget *widget);

/* Generic property accessors, used heavily by animation.
 */
int ps_widget_get_property(int *v,const struct ps_widget *widget,int k);
int ps_widget_set_property(struct ps_widget *widget,int k,int v);
int ps_widget_get_property_type(const struct ps_widget *widget,int k);

/* Draw a widget to the currently-attached framebuffer.
 * For convenience, caller must track the parent's absolute position.
 */
int ps_widget_draw(struct ps_widget *widget,int parentx,int parenty);

/* The default implementation of draw calls these two functions, which you might like to use too.
 */
int ps_widget_draw_background(struct ps_widget *widget,int parentx,int parenty);
int ps_widget_draw_children(struct ps_widget *widget,int parentx,int parenty);

/* Report to the caller what size we would prefer, given a limit.
 */
int ps_widget_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh);

/* Caller has established our bounding box.
 * Set bounds of our children and pack them recursively.
 */
int ps_widget_pack(struct ps_widget *widget);

/* If you implement update(), it's all up to you.
 * You'll probably want to call this.
 * The default implementation is just this.
 */
int ps_widget_update_children(struct ps_widget *widget);

/* Primitive input events.
 */
int ps_widget_update(struct ps_widget *widget);
int ps_widget_mousemotion(struct ps_widget *widget,int x,int y);
int ps_widget_mousebutton(struct ps_widget *widget,int btnid,int value);
int ps_widget_mousewheel(struct ps_widget *widget,int dx,int dy);
int ps_widget_key(struct ps_widget *widget,int keycode,int codepoint,int value);
int ps_widget_userinput(struct ps_widget *widget,int plrid,int btnid,int value);

/* Digested input events.
 */
int ps_widget_mouseenter(struct ps_widget *widget);
int ps_widget_mouseexit(struct ps_widget *widget);
int ps_widget_activate(struct ps_widget *widget);
int ps_widget_cancel(struct ps_widget *widget);
int ps_widget_adjust(struct ps_widget *widget,int d);
int ps_widget_focus(struct ps_widget *widget);
int ps_widget_unfocus(struct ps_widget *widget);

#endif
