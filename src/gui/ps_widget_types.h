/* ps_widget_types.h
 * Public header for specific widget types.
 */

#ifndef PS_WIDGET_TYPES_H
#define PS_WIDGET_TYPES_H

extern const struct ps_widget_type ps_widget_type_root; // Only for pages' root widgets. One full-size child.
extern const struct ps_widget_type ps_widget_type_label; // Single row of text.
extern const struct ps_widget_type ps_widget_type_packer; // Single column or row of widgets.
extern const struct ps_widget_type ps_widget_type_menu; // Vertical packer of labels with selection.

/* Root.
 *****************************************************************************/

int ps_widget_root_set_page(struct ps_widget *widget,struct ps_page *page);
struct ps_page *ps_widget_root_get_page(const struct ps_widget *widget);

/* Label.
 *****************************************************************************/

int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc);

/* Packer.
 *****************************************************************************/

int ps_widget_packer_setup(
  struct ps_widget *widget,
  int axis,
  int align_major,
  int align_minor,
  int border,
  int spacing
);

/* Menu.
 *****************************************************************************/

int ps_widget_menu_add_option(struct ps_widget *widget,const char *text,int textc);
int ps_widget_menu_move_cursor(struct ps_widget *widget,int d);
int ps_widget_menu_get_cursor(const struct ps_widget *widget);

#endif
