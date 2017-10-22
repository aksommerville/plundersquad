/* ps_widget_types.h
 * Public header for specific widget types.
 */

#ifndef PS_WIDGET_TYPES_H
#define PS_WIDGET_TYPES_H

extern const struct ps_widget_type ps_widget_type_root;
extern const struct ps_widget_type ps_widget_type_label;
extern const struct ps_widget_type ps_widget_type_packer;

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

#endif
