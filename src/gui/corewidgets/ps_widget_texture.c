/* ps_widget_texture.c
 * Renders an AKGL texture across the entire widget.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

/* Object definition.
 */

struct ps_widget_texture {
  struct ps_widget hdr;
  struct akgl_texture *texture;
};

#define WIDGET ((struct ps_widget_texture*)widget)

/* Delete.
 */

static void _ps_texture_del(struct ps_widget *widget) {
  akgl_texture_del(WIDGET->texture);
}

/* Draw.
 */

static int _ps_texture_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (WIDGET->texture) {
    if (ps_video_flush_cached_drawing()<0) return -1;
    if (ps_video_draw_texture(WIDGET->texture,parentx+widget->x,parenty+widget->y,widget->w,widget->h)<0) return -1;
  }
  return 0;
}

/* Measure.
 */

static int _ps_texture_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (WIDGET->texture) {
    if (akgl_texture_get_size(w,h,WIDGET->texture)<0) return -1;
  } else {
    *w=*h=0;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_texture={

  .name="texture",
  .objlen=sizeof(struct ps_widget_texture),

  .del=_ps_texture_del,

  .draw=_ps_texture_draw,
  .measure=_ps_texture_measure,

};

/* Set texture.
 */
 
int ps_widget_texture_set_texture(struct ps_widget *widget,struct akgl_texture *texture) {
  if (!widget||(widget->type!=&ps_widget_type_texture)) return -1;
  if (texture&&(akgl_texture_ref(texture)<0)) return -1;
  akgl_texture_del(WIDGET->texture);
  WIDGET->texture=texture;
  return 0;
}
