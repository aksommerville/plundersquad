#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

/* Object definition.
 */

struct ps_widget_icon {
  struct ps_widget hdr;
  uint8_t tsid;
  uint8_t tileid;
};

#define WIDGET ((struct ps_widget_icon*)widget)

/* Draw.
 */

static int _ps_icon_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  struct akgl_vtx_mintile vtx={
    .x=parentx+widget->x+(widget->w>>1),
    .y=parenty+widget->y+(widget->h>>1),
    .tileid=WIDGET->tileid,
  };
  if (ps_video_draw_mintile(&vtx,1,WIDGET->tsid)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_icon_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_TILESIZE;
  *h=PS_TILESIZE;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_icon={

  .name="icon",
  .objlen=sizeof(struct ps_widget_icon),

  .draw=_ps_icon_draw,
  .measure=_ps_icon_measure,

};

/* Set tile.
 */
 
int ps_widget_icon_set_tile(struct ps_widget *widget,uint16_t tileid) {
  if (!widget||(widget->type!=&ps_widget_type_icon)) return -1;
  WIDGET->tsid=tileid>>8;
  WIDGET->tileid=tileid;
  return 0;
}
