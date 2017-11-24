/* ps_widget_resedit.c
 * Master view for resource editors.
 * We don't know how to load resources and we don't know anything about the data types.
 *
 * What we do:
 *  - Display resource ID.
 *  - Provide for stepping across the list of resources.
 *  - Hand off most of the screen to a type-specific editor.
 *  - Provide a "return to menu" control.
 *  - Provide a skeleton menu bar for the controller to populate.
 *
 * Children:
 *  [0] Menu bar.
 *  [1] Content. Transient, provided by controller.
 */

#include "ps.h"
#include "gui/ps_gui.h"

#define PS_RESEDIT_MENUBAR_HEIGHT 12

/* Widget definition.
 */

struct ps_widget_resedit {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_resedit*)widget)

/* Delete.
 */

static void _ps_resedit_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_resedit_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_reseditmenu))) return -1;

  if (!(child=ps_widget_spawn_label(widget,"Content not loaded",-1,0x000000ff))) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_resedit_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_resedit_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_resedit_pack(struct ps_widget *widget) {

  if (widget->childc!=2) return -1;
  struct ps_widget *menubar=widget->childv[0];
  struct ps_widget *content=widget->childv[1];

  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=PS_RESEDIT_MENUBAR_HEIGHT;

  content->x=0;
  content->y=PS_RESEDIT_MENUBAR_HEIGHT;
  content->w=widget->w;
  content->h=widget->h-content->y;

  if (ps_widget_pack(menubar)<0) return -1;
  if (ps_widget_pack(content)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_resedit={
  .name="resedit",
  .objlen=sizeof(struct ps_widget_resedit),
  .del=_ps_resedit_del,
  .init=_ps_resedit_init,
  .draw=_ps_resedit_draw,
  .measure=_ps_resedit_measure,
  .pack=_ps_resedit_pack,
};
