#include "ps.h"
#include "gui/ps_gui.h"
#include "video/ps_video.h"

/* Widget definition.
 */

struct ps_widget_label {
  struct ps_widget hdr;
  char *text;
  int textc;
  int size;
  int xalign,yalign; // -1,0,1
  int font;
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
  WIDGET->font=7;
  WIDGET->size=12;
  widget->fgrgba=0xffffffff;
  return 0;
}

/* Get starting position for text rendering, including my own (x,y).
 */

static void ps_label_get_start_position(int *x,int *y,const struct ps_widget *widget) {
  
  if (WIDGET->xalign<0) {
    *x=widget->x;
  } else if (WIDGET->xalign>0) {
    *x=widget->x+widget->w-WIDGET->textc*(WIDGET->size>>1);
  } else {
    *x=widget->x+(widget->w>>1)-((WIDGET->textc*(WIDGET->size>>1))>>1);
  }
  *x+=WIDGET->size>>2;

  if (WIDGET->yalign<0) {
    *y=widget->y;
  } else if (WIDGET->yalign>0) {
    *y=widget->y+widget->h-WIDGET->size;
  } else {
    *y=widget->y+(widget->h>>1)-(WIDGET->size>>1);
  }
  *y+=WIDGET->size>>1;

}

/* Draw.
 * TODO Is it possible to draw multiple labels in a single rendering pass?
 */

static int _ps_label_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (WIDGET->textc) {
    int x,y;
    ps_label_get_start_position(&x,&y,widget);
    x+=x0;
    y+=y0;
    if (ps_video_text_begin()<0) return -1;
    if (ps_video_text_add(WIDGET->size,widget->fgrgba,x,y,WIDGET->text,WIDGET->textc)<0) return -1;
    if (ps_video_text_end(WIDGET->font)<0) return -1;
  }
  // Label must not have children.
  return 0;
}

/* Measure.
 */

static int _ps_label_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=WIDGET->textc*(WIDGET->size>>1);
  *h=WIDGET->size;
  return 0;
}

/* Pack.
 */

static int _ps_label_pack(struct ps_widget *widget) {
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
  .pack=_ps_label_pack,
};

/* Set text.
 */
 
int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_label)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>1024) srcc=1024; // Sanity limit.
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (WIDGET->text) free(WIDGET->text);
  WIDGET->text=nv;
  WIDGET->textc=srcc;
  return 0;
}
