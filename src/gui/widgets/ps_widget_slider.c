#include "ps.h"
#include "gui/ps_gui.h"
#include "video/ps_video.h"

#define PS_SLIDER_DEFAULT_WIDTH 80

/* Widget definition.
 */

struct ps_widget_slider {
  struct ps_widget hdr;
  int lo,hi;
  int v;
  int boxx,boxy,boxw,boxh;
  uint32_t boxrgba;
  uint32_t thumbrgba;
};

#define WIDGET ((struct ps_widget_slider*)widget)

/* Delete.
 */

static void _ps_slider_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_slider_init(struct ps_widget *widget) {

  WIDGET->boxrgba=  0x808080c0;
  WIDGET->thumbrgba=0xffffffff;

  struct ps_widget *label=ps_widget_spawn_label(widget,"",0,0xffffffff);
  if (!label) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_slider_draw(struct ps_widget *widget,int x0,int y0) {

  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;

  if (ps_video_draw_rect(x0+widget->x+WIDGET->boxx,y0+widget->y+WIDGET->boxy,WIDGET->boxw,WIDGET->boxh,WIDGET->boxrgba)<0) return -1;

  if ((WIDGET->v>=WIDGET->lo)&&(WIDGET->v<=WIDGET->hi)) {
    int thumbx=((WIDGET->v-WIDGET->lo)*WIDGET->boxw)/(WIDGET->hi-WIDGET->lo+1);
    thumbx+=WIDGET->boxx;
    int thumbw=WIDGET->boxh;
    if (thumbw>WIDGET->boxw) thumbw=WIDGET->boxw;
    if (thumbx<WIDGET->boxx) thumbx=WIDGET->boxx;
    if (thumbx+thumbw>WIDGET->boxx+WIDGET->boxw) thumbx=WIDGET->boxx+WIDGET->boxw-thumbw;
    if (ps_video_draw_rect(x0+widget->x+thumbx,y0+widget->y+WIDGET->boxy,thumbw,WIDGET->boxh,WIDGET->thumbrgba)<0) return -1;
  }

  return 0;
}

/* Measure.
 */

static int _ps_slider_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (widget->childc!=1) return -1;

  int labelw,labelh;
  if (ps_widget_measure(&labelw,&labelh,widget->childv[0],maxw,maxh)<0) return -1;

  *w=labelw+PS_SLIDER_DEFAULT_WIDTH;
  *h=labelh;
  
  return 0;
}

/* Pack.
 */

static int _ps_slider_pack(struct ps_widget *widget) {
  if (widget->childc!=1) return -1;

  struct ps_widget *label=widget->childv[0];
  int labelw,labelh;
  if (ps_widget_measure(&labelw,&labelh,label,widget->w,widget->h)<0) return -1;
  label->x=0;
  label->y=0;
  label->w=labelw;
  label->h=widget->h;
  if (ps_widget_pack(label)<0) return -1;

  WIDGET->boxx=labelw;
  WIDGET->boxw=widget->w-labelw;
  WIDGET->boxy=0;
  WIDGET->boxh=widget->h;

  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_slider={
  .name="slider",
  .objlen=sizeof(struct ps_widget_slider),
  .del=_ps_slider_del,
  .init=_ps_slider_init,
  .draw=_ps_slider_draw,
  .measure=_ps_slider_measure,
  .pack=_ps_slider_pack,
};

/* Setup.
 */
 
int ps_widget_slider_setup(struct ps_widget *widget,const char *text,int textc,int lo,int hi) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (widget->childc!=1) return -1;
  if (lo>hi) return -1;
  if (ps_widget_label_set_text(widget->childv[0],text,textc)<0) return -1;
  WIDGET->lo=lo;
  WIDGET->hi=hi;
  return 0;
}

/* Adjust.
 */
 
int ps_widget_slider_adjust(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  int nv=WIDGET->v+d;
  if (nv<WIDGET->lo) nv=WIDGET->lo;
  else if (nv>WIDGET->hi) nv=WIDGET->hi;
  WIDGET->v=nv;
  return 0;
}

/* Accessors.
 */
 
int ps_widget_slider_set_value(struct ps_widget *widget,int v) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (v<WIDGET->lo) WIDGET->v=WIDGET->lo;
  else if (v>WIDGET->hi) WIDGET->v=WIDGET->hi;
  else WIDGET->v=v;
  return 0;
}

int ps_widget_slider_get_value(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return 0;
  return WIDGET->v;
}
