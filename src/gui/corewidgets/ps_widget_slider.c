/* ps_widget_slider.c
 * Horizontal slider with text label to the left.
 * For setting integer fields (eg scenario difficulty and length).
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "video/ps_video.h"

#define PS_SLIDER_BAR_WIDTH    100
#define PS_SLIDER_BAR_HEIGHT    12
#define PS_SLIDER_SPACING        5 /* Between label and bar. */
#define PS_SLIDER_LABEL_LIMIT   20

/* Object definition.
 */

struct ps_widget_slider {
  struct ps_widget hdr;
  char *text;
  int textc;
  int lo,hi,v;
  int increment;
  uint32_t textcolor;
  int textsize;
  uint32_t locolor,hicolor;
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_slider*)widget)

/* Delete.
 */

static void _ps_slider_del(struct ps_widget *widget) {
  if (WIDGET->text) free(WIDGET->text);
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_slider_init(struct ps_widget *widget) {

  widget->bgrgba=0x404040ff;
  widget->fgrgba=0xffff00ff;
  WIDGET->textcolor=0x000000ff;
  WIDGET->textsize=12;

  return 0;
}

/* General properties.
 */

static int _ps_slider_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_slider_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_slider_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Blend colors.
 */

static uint32_t ps_rgba_blend(uint32_t a,uint32_t b,int p,int c) {
  if (c<1) return a;
  if (p<=0) return a;
  if (p>=c-1) return b;
  uint32_t dst;
  uint8_t *av=(uint8_t*)&a;
  uint8_t *bv=(uint8_t*)&b;
  uint8_t *dstv=(uint8_t*)&dst;
  int i; for (i=0;i<4;i++) {
    int v=(av[i]*(c-p)+bv[i]*p)/c;
    if (v<=0) dstv[i]=0;
    else if (v>=255) dstv[i]=255;
    else dstv[i]=v;
  }
  return dst;
}

/* Draw.
 */

static int _ps_slider_draw(struct ps_widget *widget,int parentx,int parenty) {

  /* Bar's size is my full height and a constant width, at the right. */
  int barx=parentx+widget->x+widget->w-PS_SLIDER_BAR_WIDTH;
  int bary=parenty+widget->y;
  int barw=PS_SLIDER_BAR_WIDTH;
  int barh=widget->h;
  if (ps_video_draw_rect(barx,bary,barw,barh,widget->bgrgba)<0) return -1;

  /* Calculate size and position of thumb. */
  int valuec=WIDGET->hi-WIDGET->lo+1; // How many possible values?
  if (valuec<1) valuec=1;
  int valuep=WIDGET->v-WIDGET->lo; // Normalized value, in case (lo) is not zero.
  if (valuep<0) valuep=0; else if (valuep>=valuec) valuep=valuec-1;
  int thumbw=barw/valuec; // With few values, thumb is proportionately wide.
  if (thumbw<barh) thumbw=barh; // Thumb is never narrower than square.
  if (thumbw>barw) thumbw=barw; // Probably not possible, but keep the thumb size within the bar size.
  int thumblimit=barw-thumbw; // Maximum position for thumb's left edge.
  int thumbx;
  if (valuec<2) {
    thumbx=0;
  } else {
    thumbx=(valuep*thumblimit)/(valuec-1);
    if (thumbx<0) thumbx=0;
    else if (thumbx>thumblimit) thumbx=thumblimit;
  }
  thumbx+=barx; // Offset by bar's position.
  
  uint32_t thumbcolor=widget->fgrgba;
  if (WIDGET->locolor||WIDGET->hicolor) {
    thumbcolor=ps_rgba_blend(WIDGET->locolor,WIDGET->hicolor,WIDGET->v-WIDGET->lo,WIDGET->hi-WIDGET->lo+1);
  }
  if (ps_video_draw_rect(thumbx,bary,thumbw,barh,thumbcolor)<0) return -1;

  /* Label is left of the bar. */
  if (WIDGET->textc>0) {
    int textw=WIDGET->textc*(WIDGET->textsize>>1);
    int textx=barx-PS_SLIDER_SPACING-textw+(WIDGET->textsize>>2);
    int texty=parenty+widget->y+(widget->h>>1);
    if (ps_video_text_begin()<0) return -1;
    if (ps_video_text_add(WIDGET->textsize,WIDGET->textcolor,textx,texty,WIDGET->text,WIDGET->textc)<0) return -1;
    if (ps_video_text_end(-1)<0) return -1;
  }

  return 0;
}

/* Measure.
 */

static int _ps_slider_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SLIDER_BAR_WIDTH+PS_SLIDER_SPACING+(WIDGET->textc*(WIDGET->textsize>>1));
  *h=PS_SLIDER_BAR_HEIGHT;
  if (WIDGET->textsize>*h) *h=WIDGET->textsize;
  return 0;
}

/* Digested input events.
 */

static int _ps_slider_adjust(struct ps_widget *widget,int d) {
  if (d<0) d=-WIDGET->increment;
  else if (d>0) d=WIDGET->increment;
  WIDGET->v+=d;
  if (WIDGET->v<WIDGET->lo) WIDGET->v=WIDGET->lo;
  else if (WIDGET->v>WIDGET->hi) WIDGET->v=WIDGET->hi;
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_slider={

  .name="slider",
  .objlen=sizeof(struct ps_widget_slider),

  .del=_ps_slider_del,
  .init=_ps_slider_init,

  .get_property=_ps_slider_get_property,
  .set_property=_ps_slider_set_property,
  .get_property_type=_ps_slider_get_property_type,

  .draw=_ps_slider_draw,
  .measure=_ps_slider_measure,

  .adjust=_ps_slider_adjust,

};

/* Accessors.
 */
 
int ps_widget_slider_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}
 
int ps_widget_slider_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>PS_SLIDER_LABEL_LIMIT) srcc=PS_SLIDER_LABEL_LIMIT;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (WIDGET->text) free(WIDGET->text);
  WIDGET->text=nv;
  WIDGET->textc=srcc;
  return 0;
}

int ps_widget_slider_set_limits(struct ps_widget *widget,int lo,int hi) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (lo>hi) {
    int tmp=lo;
    lo=hi;
    hi=tmp;
  }
  WIDGET->lo=lo;
  WIDGET->hi=hi;
  if (WIDGET->v<WIDGET->lo) WIDGET->v=WIDGET->lo;
  else if (WIDGET->v>WIDGET->hi) WIDGET->v=WIDGET->hi;
  return 0;
}

int ps_widget_slider_set_value(struct ps_widget *widget,int v) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (v<WIDGET->lo) WIDGET->v=WIDGET->lo;
  else if (v>WIDGET->hi) WIDGET->v=WIDGET->hi;
  else WIDGET->v=v;
  return 0;
}

int ps_widget_slider_get_value(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  return WIDGET->v;
}

int ps_widget_slider_set_increment(struct ps_widget *widget,int increment) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  if (increment<0) increment=1;
  WIDGET->increment=increment;
  return 0;
}

int ps_widget_slider_set_text_color(struct ps_widget *widget,uint32_t rgba) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  WIDGET->textcolor=rgba;
  return 0;
}

int ps_widget_slider_set_variable_color(struct ps_widget *widget,uint32_t locolor,uint32_t hicolor) {
  if (!widget||(widget->type!=&ps_widget_type_slider)) return -1;
  WIDGET->locolor=locolor;
  WIDGET->hicolor=hicolor;
  return 0;
}
