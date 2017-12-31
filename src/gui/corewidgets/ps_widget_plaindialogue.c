/* ps_widget_plaindialogue.c
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"

#define PS_PLAINDIALOGUE_MARGIN 5

/* Object definition.
 */

struct ps_widget_plaindialogue {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_plaindialogue*)widget)

/* Delete.
 */

static void _ps_plaindialogue_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_plaindialogue_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;

  return 0;
}

/* Measure.
 */

static int _ps_plaindialogue_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {

  *w=*h=0;
  int i=widget->childc; while (i-->0) {
    int chw,chh;
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
    if (chw>*w) *w=chw;
    if (chh>*h) *h=chh;
  }
  
  (*w)+=PS_PLAINDIALOGUE_MARGIN<<1;
  (*h)+=PS_PLAINDIALOGUE_MARGIN<<1;

  /* Our child might be arbitrarily sized, so cut it off at a sensible limit.
   */
  int wlimit=(PS_SCREENW*2)/3;
  int hlimit=(PS_SCREENH*2)/3;
  if (*w>wlimit) *w=wlimit;
  if (*h>hlimit) *h=hlimit;
  
  return 0;
}

/* Pack.
 */

static int _ps_plaindialogue_pack(struct ps_widget *widget) {
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    child->x=PS_PLAINDIALOGUE_MARGIN;
    child->y=PS_PLAINDIALOGUE_MARGIN;
    child->w=widget->w-(PS_PLAINDIALOGUE_MARGIN<<1);
    child->h=widget->h-(PS_PLAINDIALOGUE_MARGIN<<1);
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_plaindialogue={

  .name="plaindialogue",
  .objlen=sizeof(struct ps_widget_plaindialogue),

  .del=_ps_plaindialogue_del,
  .init=_ps_plaindialogue_init,

  .measure=_ps_plaindialogue_measure,
  .pack=_ps_plaindialogue_pack,

};

/* Dismiss.
 */
 
int ps_widget_plaindialogue_dismiss(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_plaindialogue)) widget=widget->parent;
  return ps_widget_kill(widget);
}
