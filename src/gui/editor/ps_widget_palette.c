/* ps_widget_palette.c
 * List item in editplrdef.
 * This is not interactive. Instead, we depend on the parent list to invoke editor.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "video/ps_video.h"

/* Object definition.
 */

struct ps_widget_palette {
  struct ps_widget hdr;
  uint32_t rgba_head;
  uint32_t rgba_body;
};

#define WIDGET ((struct ps_widget_palette*)widget)

/* Delete.
 */

static void _ps_palette_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_palette_init(struct ps_widget *widget) {
  widget->bgrgba=0x808080ff;
  return 0;
}

/* Draw.
 */

static int _ps_palette_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  int midx=widget->w>>1;
  if (ps_video_draw_rect(parentx+widget->x+1,parenty+widget->y+1,midx-2,widget->h-2,WIDGET->rgba_head)<0) return -1;
  if (ps_video_draw_rect(parentx+widget->x+midx+1,parenty+widget->y+1,widget->w-midx-2,widget->h-2,WIDGET->rgba_body)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_palette_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=16;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_palette={

  .name="palette",
  .objlen=sizeof(struct ps_widget_palette),

  .del=_ps_palette_del,
  .init=_ps_palette_init,

  .draw=_ps_palette_draw,
  .measure=_ps_palette_measure,

};

/* Setup.
 */
 
int ps_widget_palette_setup(struct ps_widget *widget,uint32_t rgba_head,uint32_t rgba_body) {
  if (!widget||(widget->type!=&ps_widget_type_palette)) return -1;
  WIDGET->rgba_head=rgba_head;
  WIDGET->rgba_body=rgba_body;
  return 0;
}
