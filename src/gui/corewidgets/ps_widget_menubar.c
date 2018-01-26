#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"

#define PS_MENUBAR_DEFAULT_HEIGHT 16

/* Object definition.
 */

struct ps_widget_menubar {
  struct ps_widget hdr;
  int hpadding;
  int vpadding;
  int spacing;
};

#define WIDGET ((struct ps_widget_menubar*)widget)

/* Delete.
 */

static void _ps_menubar_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_menubar_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;
  WIDGET->hpadding=5;
  WIDGET->vpadding=0;
  WIDGET->spacing=5;
  
  return 0;
}

/* Measure.
 */

static int _ps_menubar_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {

  if (widget->childc<1) {
    *w=maxw;
    *h=PS_MENUBAR_DEFAULT_HEIGHT;
    return 0;
  }

  *w=*h=0;

  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
    (*w)+=chw;
    if (chh>*h) *h=chh;
  }
  
  (*w)+=WIDGET->hpadding<<1;
  (*w)+=WIDGET->spacing*(widget->childc-1);
  (*h)+=WIDGET->vpadding<<1;
  return 0;
}

/* Pack.
 */

static int _ps_menubar_pack(struct ps_widget *widget) {

  int x=WIDGET->hpadding;
  int y=WIDGET->vpadding;
  int availh=widget->h-(WIDGET->vpadding<<1);
  if (availh<0) availh=0;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,widget->w-x,availh)<0) return -1;
    child->x=x;
    child->y=y;
    child->w=chw;
    child->h=availh;
    if (ps_widget_pack(child)<0) return -1;
    x+=chw;
    x+=WIDGET->spacing;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_menubar={

  .name="menubar",
  .objlen=sizeof(struct ps_widget_menubar),

  .del=_ps_menubar_del,
  .init=_ps_menubar_init,

  .measure=_ps_menubar_measure,
  .pack=_ps_menubar_pack,

};

/* Force label to last child (eg after adding button).
 */

static int ps_menubar_force_label_to_end(struct ps_widget *widget) {
  int i=widget->childc-1; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (child->type==&ps_widget_type_label) {
      memmove(widget->childv+i,widget->childv+i+1,sizeof(void*)*(widget->childc-i-1));
      widget->childv[widget->childc-1]=child;
      return 0;
    }
  }
  return 0;
}

/* Add button.
 */
 
struct ps_widget *ps_widget_menubar_add_button(struct ps_widget *widget,const char *text,int textc,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_menubar)) return 0;
  struct ps_widget *button=ps_widget_button_spawn(widget,0,text,textc,cb);
  if (!button) return 0;
  if (ps_widget_button_set_margins(button,0,0)<0) return 0;
  if (ps_menubar_force_label_to_end(widget)<0) return 0;
  return button;
}

/* Set title.
 */

static struct ps_widget *ps_widget_menubar_require_title(struct ps_widget *widget) {
  if ((widget->childc>0)&&(widget->childv[widget->childc-1]->type==&ps_widget_type_label)) return widget->childv[widget->childc-1];
  struct ps_widget *label=ps_widget_spawn(widget,&ps_widget_type_label);
  if (!label) return 0;
  label->fgrgba=0xc0c0c0ff;
  return label;
}
 
struct ps_widget *ps_widget_menubar_set_title(struct ps_widget *widget,const char *text,int textc) {
  if (!widget||(widget->type!=&ps_widget_type_menubar)) return 0;
  struct ps_widget *label=ps_widget_menubar_require_title(widget);
  if (ps_widget_label_set_text(label,text,textc)<0) return 0;
  return label;
}
