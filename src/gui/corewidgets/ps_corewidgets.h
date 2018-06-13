/* ps_corewidgets.h
 * Interface to widgets that are a fundamental part of the GUI's plumbing.
 * The widget classes defined here are not specific to any particular screen.
 */

#ifndef PS_COREWIDGETS_H
#define PS_COREWIDGETS_H

#include "util/ps_callback.h"

struct akgl_texture;

extern const struct ps_widget_type ps_widget_type_root; /* The master widget. */

extern const struct ps_widget_type ps_widget_type_blotter; /* Solid color, no interaction. */
extern const struct ps_widget_type ps_widget_type_label; /* Single line text. */
extern const struct ps_widget_type ps_widget_type_icon; /* Graphic tile. */
extern const struct ps_widget_type ps_widget_type_texture; /* Large image. */
extern const struct ps_widget_type ps_widget_type_button; /* Clickable button. */
extern const struct ps_widget_type ps_widget_type_toggle; /* State-preserving button. */
extern const struct ps_widget_type ps_widget_type_field; /* Single line text entry. */
extern const struct ps_widget_type ps_widget_type_textblock; /* Multi-line static text. */
extern const struct ps_widget_type ps_widget_type_slider; /* Horizontal slider with text label, for integer input. */

extern const struct ps_widget_type ps_widget_type_packer; /* Lines children up in a row or column. */
extern const struct ps_widget_type ps_widget_type_menu; /* List of selectable options. */
extern const struct ps_widget_type ps_widget_type_scrolllist; /* Vertically scrolling list of arbitrary widgets. */
extern const struct ps_widget_type ps_widget_type_dialogue; /* Box with optional message, input, and buttons. */
extern const struct ps_widget_type ps_widget_type_plaindialogue; /* Agnostic dialogue box, put whatever you want in it. */
extern const struct ps_widget_type ps_widget_type_menubar; /* Horizontal bar of buttons. */
extern const struct ps_widget_type ps_widget_type_scrollbar; /* General-purpose scrollbar. */

/* Root.
 *****************************************************************************/

/* The root widget holds a weak reference to the GUI that owns it.
 */
struct ps_gui *ps_widget_root_get_gui(const struct ps_widget *widget);
int ps_widget_root_set_gui(struct ps_widget *widget,struct ps_gui *gui);

/* Root handles keyboard focus on its own, as far as the Tab key goes.
 * Root does NOT handle mouse clicks in a keyboard-focusable widget.
 * For that, you must catch those mouse events yourself and request the keyboard focus manually.
 */
int ps_widget_root_request_keyboard_focus(struct ps_widget *widget,struct ps_widget *focus);

/* Label.
 *****************************************************************************/

struct ps_widget *ps_widget_label_spawn(struct ps_widget *parent,const char *src,int srcc);
const char *ps_widget_label_get_text(const struct ps_widget *widget);
int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc);
int ps_widget_label_set_textfv(struct ps_widget *widget,const char *fmt,va_list vargs);
int ps_widget_label_set_textf(struct ps_widget *widget,const char *fmt,...);
int ps_widget_label_set_font_resid(struct ps_widget *widget,int resid);
int ps_widget_label_set_size(struct ps_widget *widget,int size);

/* Icon.
 *****************************************************************************/

int ps_widget_icon_set_tile(struct ps_widget *widget,uint16_t tileid);

int ps_widget_texture_set_texture(struct ps_widget *widget,struct akgl_texture *texture);

/* Button.
 *****************************************************************************/

struct ps_widget *ps_widget_button_spawn(
  struct ps_widget *parent,
  uint16_t tileid,
  const char *text,int textc,
  struct ps_callback cb
);

int ps_widget_button_set_callback(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_button_set_margins(struct ps_widget *widget,int bevel_width,int border_width);
int ps_widget_button_set_icon(struct ps_widget *widget,uint16_t tileid);
int ps_widget_button_set_text(struct ps_widget *widget,const char *src,int srcc);
const char *ps_widget_button_get_text(const struct ps_widget *widget);
int ps_widget_button_set_text_color(struct ps_widget *widget,uint32_t rgba);

/* Toggle.
 *****************************************************************************/

// Setting value manually does not fire the callback.
int ps_widget_toggle_set_value(struct ps_widget *widget,int value);
int ps_widget_toggle_get_value(const struct ps_widget *widget);

int ps_widget_toggle_set_enable(struct ps_widget *widget,int enable);
int ps_widget_toggle_get_enable(const struct ps_widget *widget);

// Toggles may have a label or an icon but not both.
// Setting one clears the other.
struct ps_widget *ps_widget_toggle_set_text(struct ps_widget *widget,const char *src,int srcc);
struct ps_widget *ps_widget_toggle_set_icon(struct ps_widget *widget,uint16_t tileid);

int ps_widget_toggle_set_callback(struct ps_widget *widget,struct ps_callback cb);

/* Field.
 *****************************************************************************/

int ps_widget_field_set_text(struct ps_widget *widget,const char *src,int srcc);
int ps_widget_field_get_text(void *dstpp,const struct ps_widget *widget);

int ps_widget_field_set_integer(struct ps_widget *widget,int src);
int ps_widget_field_get_integer(int *dst,const struct ps_widget *widget);

int ps_widget_field_append_char(struct ps_widget *widget,int codepoint);
int ps_widget_field_backspace(struct ps_widget *widget);
int ps_widget_field_delete(struct ps_widget *widget);
int ps_widget_field_move_cursor(struct ps_widget *widget,int d);

int ps_widget_field_set_cb_change(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_field_set_cb_blur(struct ps_widget *widget,struct ps_callback cb);

/* Textblock.
 *****************************************************************************/

int ps_widget_textblock_set_text(struct ps_widget *widget,const char *src,int srcc);

/* (-1,0,1) = (left/top),center,(right/bottom).
 * Default is (-1,-1).
 */
int ps_widget_textblock_set_alignment(struct ps_widget *widget,int horz,int vert);

/* Packer.
 *****************************************************************************/

/* (padding) is around the edge of the packer.
 * (spacing) is between the children.
 * Alignments are from <util/ps_geometry.h>, PS_ALIGN_{START,CENTER,END,FILL}.
 * Axes are from <util/ps_geometry.h>, PS_AXIS_VERT or PS_AXIS_HORZ.
 */
int ps_widget_packer_set_margins(struct ps_widget *widget,int padding,int spacing);
int ps_widget_packer_set_alignment(struct ps_widget *widget,int major,int minor);
int ps_widget_packer_set_axis(struct ps_widget *widget,int axis);

/* Menu.
 *****************************************************************************/

struct ps_widget *ps_widget_menu_get_thumb(const struct ps_widget *widget);
struct ps_widget *ps_widget_menu_get_packer(const struct ps_widget *widget);

int ps_widget_menu_set_callback(struct ps_widget *widget,struct ps_callback cb);

int ps_widget_menu_get_selected_index(const struct ps_widget *widget);
struct ps_widget *ps_widget_menu_get_selected_widget(const struct ps_widget *widget);

int ps_widget_menu_change_selection(struct ps_widget *widget,int d);
int ps_widget_menu_adjust_selection(struct ps_widget *widget,int d);
int ps_widget_menu_activate(struct ps_widget *widget);

/* Button callbacks will not fire if you have set a callback on the menu.
 */
struct ps_widget *ps_widget_menu_spawn_label(struct ps_widget *widget,const char *src,int srcc);
struct ps_widget *ps_widget_menu_spawn_button(struct ps_widget *widget,const char *src,int srcc,struct ps_callback cb);

/* Scrolllist.
 *****************************************************************************/

#define PS_WIDGET_SCROLLLIST_PROPERTY_scroll 1000

int ps_widget_scrolllist_get_scroll_position(const struct ps_widget *widget);
int ps_widget_scrolllist_set_scroll_position(struct ps_widget *widget,int p);
int ps_widget_scrolllist_adjust(struct ps_widget *widget,int dy);

struct ps_widget *ps_widget_scrolllist_add_label(struct ps_widget *widget,const char *src,int srcc);

int ps_widget_scrolllist_enable_selection(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_scrolllist_get_selection(const struct ps_widget *widget);
int ps_widget_scrolllist_set_selection(struct ps_widget *widget,int p);
int ps_widget_scrolllist_set_selection_without_callback(struct ps_widget *widget,int p);
const char *ps_widget_scrolllist_get_text(const struct ps_widget *widget);

/* Dialogue.
 *****************************************************************************/

struct ps_widget *ps_widget_dialogue_set_message(struct ps_widget *widget,const char *text,int textc);
struct ps_widget *ps_widget_dialogue_set_input(struct ps_widget *widget,const char *text,int textc);
struct ps_widget *ps_widget_dialogue_add_button(struct ps_widget *widget,const char *text,int textc,struct ps_callback cb);

int ps_widget_dialogue_get_text(void *dstpp,const struct ps_widget *widget);
int ps_widget_dialogue_get_number(int *dst,const struct ps_widget *widget);

/* This is kind of sloppy, but we give a few fields for originators to pass extra data to their callback.
 */
int ps_widget_dialogue_set_userdata(struct ps_widget *widget,void *userdata,void (*userdata_del)(void *userdata));
void *ps_widget_dialogue_get_userdata(const struct ps_widget *widget);
int ps_widget_dialogue_set_refnum1(struct ps_widget *widget,int v);
int ps_widget_dialogue_get_refnum1(const struct ps_widget *widget);

/* Conveniences that create, populate, and present a dialogue box.
 */
struct ps_widget *ps_widget_spawn_dialogue_message(
  struct ps_widget *parent,
  const char *message,int messagec,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
);
struct ps_widget *ps_widget_spawn_dialogue_text(
  struct ps_widget *parent,
  const char *message,int messagec,
  const char *input,int inputc,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
);
struct ps_widget *ps_widget_spawn_dialogue_number(
  struct ps_widget *parent,
  const char *message,int messagec,
  int input,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
);

/* Plaindialogue.
 *****************************************************************************/

// Kill the plaindialogue. You may provide any descendant of it.
int ps_widget_plaindialogue_dismiss(struct ps_widget *widget);

/* Menubar.
 *****************************************************************************/

struct ps_widget *ps_widget_menubar_add_button(struct ps_widget *widget,const char *text,int textc,struct ps_callback cb);

/* Menu has an optional title which appears in gray text after all the buttons.
 */
struct ps_widget *ps_widget_menubar_set_title(struct ps_widget *widget,const char *text,int textc);

/* Scrollbar.
 *****************************************************************************/

int ps_widget_scrollbar_set_callback(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_scrollbar_set_orientation(struct ps_widget *widget,int axis);
int ps_widget_scrollbar_set_value(struct ps_widget *widget,int value); // Does not trigger callback.
int ps_widget_scrollbar_get_value(const struct ps_widget *widget);
int ps_widget_scrollbar_set_limit(struct ps_widget *widget,int limit);
int ps_widget_scrollbar_get_limit(const struct ps_widget *widget);
int ps_widget_scrollbar_set_visible_size(struct ps_widget *widget,int visible_size);
int ps_widget_scrollbar_get_visible_size(const struct ps_widget *widget);

int ps_widget_scrollbar_adjust(struct ps_widget *widget,int d); // Triggers callback.

/* Slider.
 *****************************************************************************/

int ps_widget_slider_set_callback(struct ps_widget *widget,struct ps_callback cb);
int ps_widget_slider_set_text(struct ps_widget *widget,const char *src,int srcc);
int ps_widget_slider_set_limits(struct ps_widget *widget,int lo,int hi);
int ps_widget_slider_set_value(struct ps_widget *widget,int v);
int ps_widget_slider_get_value(const struct ps_widget *widget);
int ps_widget_slider_set_increment(struct ps_widget *widget,int increment);
int ps_widget_slider_set_text_color(struct ps_widget *widget,uint32_t rgba);

// Color of thumb blends from (locolor) to (hicolor) depending on its position.
int ps_widget_slider_set_variable_color(struct ps_widget *widget,uint32_t locolor,uint32_t hicolor);

#endif
