#include "ps.h"
#include "../ps_widget.h"
#include "video/ps_video.h"

#define PS_LABEL_SANITY_LIMIT 255

/* Object definition.
 */

struct ps_widget_label {
  struct ps_widget hdr;
  char *text;
  int textc;
  int font_resid;
  int size;
};

#define WIDGET ((struct ps_widget_label*)widget)

/* Delete.
 */

static void _ps_label_del(struct ps_widget *widget) {
  if (WIDGET->text) free(WIDGET->text);
}

/* Initialize.
 */

static int _ps_label_init(struct ps_widget *widget) {
  widget->bgrgba=0x00000000;
  widget->fgrgba=0x000000ff;
  WIDGET->font_resid=0; // <=0 defaults to the built-in minimal font.
  WIDGET->size=12;
  return 0;
}

/* Draw.
 */

static int _ps_label_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (WIDGET->textc>0) {
  
    int x=parentx+widget->x+(widget->w>>1); // Horiztonal midpoint...
    x-=((WIDGET->textc*WIDGET->size)>>2); // ...minus half of the text's width...
    x+=(WIDGET->size>>2); // ...plus half of one glyph's width.
    int y=parenty+widget->y+(widget->h>>1)+1; // Vertical midpoint. I don't know why '+1'.
    
    if (ps_video_text_begin()<0) return -1;
    if (ps_video_text_add(WIDGET->size,widget->fgrgba,x,y,WIDGET->text,WIDGET->textc)<0) return -1;
    if (ps_video_text_end(WIDGET->font_resid)<0) return -1;
    
  }
  return 0;
}

/* Measure.
 */

static int _ps_label_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=(WIDGET->textc*WIDGET->size)>>1;
  *h=WIDGET->size;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_label={

  .name="label",
  .objlen=sizeof(struct ps_widget_label),

  .del=_ps_label_del,
  .init=_ps_label_init,

  .draw=_ps_label_draw,
  .measure=_ps_label_measure,

};

/* Accessors.
 */
 
const char *ps_widget_label_get_text(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_label)) return 0;
  return WIDGET->text;
}
 
int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_label)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>PS_LABEL_SANITY_LIMIT) srcc=PS_LABEL_SANITY_LIMIT;
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

int ps_widget_label_set_textfv(struct ps_widget *widget,const char *fmt,va_list vargs) {
  char buf[256];
  int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
  if ((bufc<0)||(bufc>=sizeof(buf))) {
    return ps_widget_label_set_text(widget,0,0);
  } else {
    return ps_widget_label_set_text(widget,buf,bufc);
  }
}

int ps_widget_label_set_textf(struct ps_widget *widget,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return ps_widget_label_set_textfv(widget,fmt,vargs);
}

int ps_widget_label_set_font_resid(struct ps_widget *widget,int resid) {
  if (!widget||(widget->type!=&ps_widget_type_label)) return -1;
  WIDGET->font_resid=resid;
  return 0;
}

int ps_widget_label_set_size(struct ps_widget *widget,int size) {
  if (!widget||(widget->type!=&ps_widget_type_label)) return -1;
  if (size<2) return -1;
  WIDGET->size=size;
  return 0;
}
