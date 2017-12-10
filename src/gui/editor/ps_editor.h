/* ps_editor.h
 * Widgets specific to the data editor.
 */

#ifndef PS_EDITOR_H
#define PS_EDITOR_H

extern const struct ps_widget_type ps_widget_type_edithome;

/* Edithome.
 *****************************************************************************/

/* (widget) can be any of the resource-specific controller types.
 * It can also be a dialogue, in which case we populate it with a friendly error message.
 * This function lives in ps_widget_edithome.c.
 */
int ps_widget_editor_set_resource(struct ps_widget *widget,int id,void *obj);

#endif
