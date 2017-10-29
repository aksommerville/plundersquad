#include "ps.h"
#include "gui/ps_gui.h"

/* Widget definition.
 */

struct ps_widget_root {
  struct ps_widget hdr;
  struct ps_page *page; // WEAK
};

#define WIDGET ((struct ps_widget_root*)widget)

/* Delete.
 */

static void _ps_root_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_root_init(struct ps_widget *widget) {
  widget->bgrgba=0x000000ff; // Black is the default. Pages should override this.
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_root={
  .name="root",
  .objlen=sizeof(struct ps_widget_root),
  .del=_ps_root_del,
  .init=_ps_root_init,
  // Draw, measure, and pack all take the defaults.
};

/* Containing page.
 */
 
int ps_widget_root_set_page(struct ps_widget *widget,struct ps_page *page) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return -1;
  WIDGET->page=page;
  return 0;
}

struct ps_page *ps_widget_root_get_page(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_root)) return 0;
  return WIDGET->page;
}
