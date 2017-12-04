/* ps_corewidgets.h
 * Interface to widgets that are a fundamental part of the GUI's plumbing.
 */

#ifndef PS_COREWIDGETS_H
#define PS_COREWIDGETS_H

#include "util/ps_callback.h"

extern const struct ps_widget_type ps_widget_type_root; /* The master widget. */

extern const struct ps_widget_type ps_widget_type_blotter; /* Solid color, no interaction. */
extern const struct ps_widget_type ps_widget_type_label; /* Single line text. */
extern const struct ps_widget_type ps_widget_type_icon; /* Graphic tile. */
extern const struct ps_widget_type ps_widget_type_button; /* Clickable button. */
extern const struct ps_widget_type ps_widget_type_field; /* Single line text entry. */
extern const struct ps_widget_type ps_widget_type_textblock; /* Multi-line static text. */
extern const struct ps_widget_type ps_widget_type_packer; /* Lines children up in a row or column. */

//TODO menu: simple text menu (esp for game)
//TODO scrolllist: vertical scroller, letting children disappear above and below (esp for editor)
//TODO dialogue: modal dialogue box with optional icon, prompt, field, and buttons

/* Root.
 *****************************************************************************/

/* The root widget holds a weak reference to the GUI that owns it.
 */
struct ps_gui *ps_widget_root_get_gui(const struct ps_widget *widget);
int ps_widget_root_set_gui(struct ps_widget *widget,struct ps_gui *gui);

/* Label.
 *****************************************************************************/

int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc);
int ps_widget_label_set_font_resid(struct ps_widget *widget,int resid);
int ps_widget_label_set_size(struct ps_widget *widget,int size);

/* Icon.
 *****************************************************************************/

int ps_widget_icon_set_tile(struct ps_widget *widget,uint16_t tileid);

/* Button.
 *****************************************************************************/

struct ps_widget *ps_widget_button_spawn(
  struct ps_widget *parent,
  uint16_t tileid,
  const char *text,int textc,
  struct ps_callback cb
);

int ps_widget_button_set_callback(struct ps_widget *widget,struct ps_callback cb);

/* Field.
 *****************************************************************************/

int ps_widget_field_set_text(struct ps_widget *widget,const char *src,int srcc);

int ps_widget_field_append_char(struct ps_widget *widget,int codepoint);
int ps_widget_field_backspace(struct ps_widget *widget);
int ps_widget_field_delete(struct ps_widget *widget);
int ps_widget_field_move_cursor(struct ps_widget *widget,int d);

/* Textblock.
 *****************************************************************************/

int ps_widget_textblock_set_text(struct ps_widget *widget,const char *src,int srcc);

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

#endif
