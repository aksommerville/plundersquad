#include "ps.h"
#include "../ps_widget.h"

/* Initialize.
 */

static int _ps_blotter_init(struct ps_widget *widget) {
  widget->bgrgba=0x000000ff;
  return 0;
}

/* Draw.
 */

static int _ps_blotter_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_blotter={

  .name="blotter",
  .objlen=sizeof(struct ps_widget),

  .init=_ps_blotter_init,
  .draw=_ps_blotter_draw,

};
