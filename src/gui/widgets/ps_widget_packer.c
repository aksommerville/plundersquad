#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"

/* Widget definition.
 */

struct ps_widget_packer {
  struct ps_widget hdr;
  int axis;
  int align_major;
  int align_minor;
  int border;
  int spacing;
  struct ps_vector *prefv; // Store children's preferred size during pack.
  int prefa;
};

#define WIDGET ((struct ps_widget_packer*)widget)

/* Delete.
 */

static void _ps_packer_del(struct ps_widget *widget) {
  if (WIDGET->prefv) free(WIDGET->prefv);
}

/* Initialize.
 */

static int _ps_packer_init(struct ps_widget *widget) {

  WIDGET->axis=PS_AXIS_VERT;
  WIDGET->align_major=PS_ALIGN_FILL;
  WIDGET->align_minor=PS_ALIGN_FILL;
  WIDGET->border=0;
  WIDGET->spacing=0;

  return 0;
}

/* Measure children based on their preferred size.
 */

static int ps_packer_measure_children(int *maxw,int *sumw,int *maxh,int *sumh,const struct ps_widget *widget,int limitw,int limith) {
  if (maxw) *maxw=0;
  if (sumw) *sumw=0;
  if (maxh) *maxh=0;
  if (sumh) *sumh=0;
  int i=widget->childc;
  while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    int cw,ch;
    if (ps_widget_measure(&cw,&ch,child,limitw,limith)<0) return -1;
    if (maxw&&(cw>*maxw)) *maxw=cw;
    if (sumw) *sumw+=cw;
    if (maxh&&(ch>*maxh)) *maxh=ch;
    if (sumh) *sumh+=ch;
  }
  return 0;
}

/* Measure children based on their actual size.
 */

static int ps_packer_get_child_width_sum(const struct ps_widget *widget) {
  int sum=0,i=widget->childc;
  while (i-->0) sum+=widget->childv[i]->w;
  return sum;
}

static int ps_packer_get_child_height_sum(const struct ps_widget *widget) {
  int sum=0,i=widget->childc;
  while (i-->0) sum+=widget->childv[i]->h;
  return sum;
}

static int ps_packer_get_child_width_max(const struct ps_widget *widget) {
  int max=0,i=widget->childc;
  while (i-->0) {
    int q=widget->childv[i]->w;
    if (q>max) max=q;
  }
  return max;
}

static int ps_packer_get_child_height_max(const struct ps_widget *widget) {
  int max=0,i=widget->childc;
  while (i-->0) {
    int q=widget->childv[i]->h;
    if (q>max) max=q;
  }
  return max;
}

/* Measure.
 */

static int _ps_packer_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {

  /* No children, get out fast. */
  if (widget->childc<1) {
    *w=*h=WIDGET->border<<1;
    return 0;
  }

  /* Get child size preferences. */
  int cwsum,cwmax,chsum,chmax;
  if (ps_packer_measure_children(&cwmax,&cwsum,&chmax,&chsum,widget,maxw,maxh)<0) return -1;

  /* One axis gets the max, other gets the sum and spacing. */
  switch (WIDGET->axis) {
    case PS_AXIS_HORZ: {
        *w=cwsum+WIDGET->spacing*(widget->childc-1);
        *h=chmax;
      } break;
    case PS_AXIS_VERT: {
        *w=cwmax;
        *h=chsum+WIDGET->spacing*(widget->childc-1);
      } break;
    default: return -1;
  }

  /* Both axes get the border. */
  *w+=WIDGET->border<<1;
  *h+=WIDGET->border<<1;
  
  return 0;
}

/* Grow prefv to accomodate all children.
 */

static int ps_packer_prefv_require(struct ps_widget *widget) {
  if (WIDGET->prefa>=widget->childc) return 0;
  void *nv=realloc(WIDGET->prefv,sizeof(struct ps_vector)*widget->childc);
  if (!nv) return -1;
  WIDGET->prefv=nv;
  WIDGET->prefa=widget->childc;
  return 0;
}

static int ps_packer_prefv_refresh(struct ps_widget *widget) {
  if (ps_packer_prefv_require(widget)<0) return -1;
  struct ps_vector *pref=WIDGET->prefv;
  int i=0; for (;i<widget->childc;i++,pref++) {
    if (ps_widget_measure(&pref->dx,&pref->dy,widget->childv[i],widget->w,widget->h)<0) return -1;
  }
  return 0;
}

/* Pack, inner logic separated by axis.
 */

static int ps_packer_pack_fallback(struct ps_widget *widget) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    child->x=child->y=0;
    child->w=widget->w;
    child->h=widget->h;
  }
  return 0;
}

static int ps_packer_pref_w_sum(const struct ps_widget *widget) {
  int sum=0,i=widget->childc;
  while (i-->0) sum+=WIDGET->prefv[i].dx;
  return sum;
}

static int ps_packer_pref_h_sum(const struct ps_widget *widget) {
  int sum=0,i=widget->childc;
  while (i-->0) sum+=WIDGET->prefv[i].dy;
  return sum;
}

static int ps_packer_haircut_children_w(struct ps_widget *widget,int deficit) {
  int take_from_all=deficit/widget->childc;
  int take_from_any=deficit%widget->childc;
  int i=widget->childc; while (i-->0) {
    if (WIDGET->prefv[i].dx>=take_from_all) {
      WIDGET->prefv[i].dx-=take_from_all;
      if (take_from_any&&WIDGET->prefv[i].dx) {
        WIDGET->prefv[i].dx--;
        take_from_any--;
      }
    } else {
      take_from_any+=take_from_all-WIDGET->prefv[i].dx;
      WIDGET->prefv[i].dx=0;
    }
  }
  return 0;
}

static int ps_packer_haircut_children_h(struct ps_widget *widget,int deficit) {
  int take_from_all=deficit/widget->childc;
  int take_from_any=deficit%widget->childc;
  int i=widget->childc; while (i-->0) {
    if (WIDGET->prefv[i].dy>=take_from_all) {
      WIDGET->prefv[i].dy-=take_from_all;
      if (take_from_any&&WIDGET->prefv[i].dy) {
        WIDGET->prefv[i].dy--;
        take_from_any--;
      }
    } else {
      take_from_any+=take_from_all-WIDGET->prefv[i].dy;
      WIDGET->prefv[i].dy=0;
    }
  }
  return 0;
}

static int ps_packer_unhaircut_children_w(struct ps_widget *widget,int surplus) {
  int add_to_all=surplus/widget->childc;
  int add_to_any=surplus%widget->childc;
  int i=widget->childc; while (i-->0) {
    WIDGET->prefv[i].dx+=add_to_all;
    if (add_to_any) {
      WIDGET->prefv[i].dx++;
      add_to_any--;
    }
  }
  return 0;
}

static int ps_packer_unhaircut_children_h(struct ps_widget *widget,int surplus) {
  int add_to_all=surplus/widget->childc;
  int add_to_any=surplus%widget->childc;
  int i=widget->childc; while (i-->0) {
    WIDGET->prefv[i].dy+=add_to_all;
    if (add_to_any) {
      WIDGET->prefv[i].dy++;
      add_to_any--;
    }
  }
  return 0;
}

static int ps_packer_pack_horz(struct ps_widget *widget) {

  /* How much space can we provide to the children? */
  int availw=widget->w-(WIDGET->border<<1)-(WIDGET->spacing*(widget->childc-1));
  int availh=widget->h-(WIDGET->border<<1);
  if ((availw<1)||(availh<1)) return ps_packer_pack_fallback(widget);

  /* How much horizontal space do the children want? */
  int desirew=ps_packer_pref_w_sum(widget);

  /* If the children want more horizontal than we have, everybody takes a haircut. */
  while (desirew>availw) {
    ps_packer_haircut_children_w(widget,desirew-availw);
    desirew=ps_packer_pref_w_sum(widget);
  }

  /* If we are filling the major axis, pad preferences to fill. */
  if ((WIDGET->align_major==PS_ALIGN_FILL)&&(desirew<availw)) {
    ps_packer_unhaircut_children_w(widget,availw-desirew);
    desirew=availw;
  }

  /* Set the initial horizontal position. */
  int position=WIDGET->border;
  if (desirew<availw) switch (WIDGET->align_major) {
    case PS_ALIGN_CENTER: position=(widget->w>>1)-((desirew+WIDGET->spacing*(widget->childc-1))>>1); break;
    case PS_ALIGN_END: position=widget->w-WIDGET->border-(desirew+WIDGET->spacing*(widget->childc-1)); break;
  }

  /* Assign positions and sizes. */
  const struct ps_vector *pref=WIDGET->prefv;
  int i=0; for (;i<widget->childc;i++,pref++) {
    struct ps_widget *child=widget->childv[i];
    child->x=position;
    child->w=pref->dx;
    position+=pref->dx;
    position+=WIDGET->spacing;
    child->h=pref->dy;
    switch (WIDGET->align_minor) {
      case PS_ALIGN_START: child->y=WIDGET->border; break;
      case PS_ALIGN_CENTER: child->y=(widget->h>>1)-(pref->dy>>1); break;
      case PS_ALIGN_END: child->y=widget->h-WIDGET->border-pref->dy; break;
      case PS_ALIGN_FILL: child->y=WIDGET->border; child->h=availh; break;
    }
  }

  return 0; 
}

static int ps_packer_pack_vert(struct ps_widget *widget) {

  /* How much space can we provide to the children? */
  int availw=widget->w-(WIDGET->border<<1);
  int availh=widget->h-(WIDGET->border<<1)-(WIDGET->spacing*(widget->childc-1));
  if ((availw<1)||(availh<1)) return ps_packer_pack_fallback(widget);

  /* How much vertical space do the children want? */
  int desireh=ps_packer_pref_h_sum(widget);

  /* If the children want more vertical than we have, everybody takes a haircut. */
  while (desireh>availh) {
    ps_packer_haircut_children_h(widget,desireh-availh);
    desireh=ps_packer_pref_h_sum(widget);
  }

  /* If we are filling the major axis, pad preferences to fill. */
  if ((WIDGET->align_major==PS_ALIGN_FILL)&&(desireh<availh)) {
    ps_packer_unhaircut_children_h(widget,availh-desireh);
    desireh=availh;
  }

  /* Set the initial vertical position. */
  int position=WIDGET->border;
  if (desireh<availh) switch (WIDGET->align_major) {
    case PS_ALIGN_CENTER: position=(widget->h>>1)-((desireh+WIDGET->spacing*(widget->childc-1))>>1); break;
    case PS_ALIGN_END: position=widget->h-WIDGET->border-(desireh+WIDGET->spacing*(widget->childc-1)); break;
  }

  /* Assign positions and sizes. */
  const struct ps_vector *pref=WIDGET->prefv;
  int i=0; for (;i<widget->childc;i++,pref++) {
    struct ps_widget *child=widget->childv[i];
    child->y=position;
    child->h=pref->dy;
    position+=pref->dy;
    position+=WIDGET->spacing;
    child->w=pref->dx;
    switch (WIDGET->align_minor) {
      case PS_ALIGN_START: child->x=WIDGET->border; break;
      case PS_ALIGN_CENTER: child->x=(widget->w>>1)-(pref->dx>>1); break;
      case PS_ALIGN_END: child->x=widget->w-WIDGET->border-pref->dx; break;
      case PS_ALIGN_FILL: child->x=WIDGET->border; child->w=availw; break;
    }
  }

  return 0; 
}

/* Pack.
 */

static int _ps_packer_pack(struct ps_widget *widget) {

  if (widget->childc<1) return 0;

  if (ps_packer_prefv_refresh(widget)<0) return -1;

  switch (WIDGET->axis) {
    case PS_AXIS_HORZ: if (ps_packer_pack_horz(widget)<0) return -1; break;
    case PS_AXIS_VERT: if (ps_packer_pack_vert(widget)<0) return -1; break;
    default: return -1;
  }

  int i=widget->childc; while (i-->0) {
    if (ps_widget_pack(widget->childv[i])<0) return -1;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_packer={
  .name="packer",
  .objlen=sizeof(struct ps_widget_packer),
  .del=_ps_packer_del,
  .init=_ps_packer_init,
  .measure=_ps_packer_measure,
  .pack=_ps_packer_pack,
};

/* Setup properties.
 */
 
int ps_widget_packer_setup(
  struct ps_widget *widget,
  int axis,
  int align_major,
  int align_minor,
  int border,
  int spacing
) {
  if (!widget||(widget->type!=&ps_widget_type_packer)) return -1;

  switch (axis) {
    case PS_AXIS_HORZ:
    case PS_AXIS_VERT:
      break;
    default: return -1;
  }
  switch (align_major) {
    case PS_ALIGN_START:
    case PS_ALIGN_CENTER:
    case PS_ALIGN_END:
    case PS_ALIGN_FILL:
      break;
    default: return -1;
  }
  switch (align_minor) {
    case PS_ALIGN_START:
    case PS_ALIGN_CENTER:
    case PS_ALIGN_END:
    case PS_ALIGN_FILL:
      break;
    default: return -1;
  }
  if ((border<0)||(border>1000)) return -1;
  if ((spacing<0)||(spacing>1000)) return -1;

  WIDGET->axis=axis;
  WIDGET->align_major=align_major;
  WIDGET->align_minor=align_minor;
  WIDGET->border=border;
  WIDGET->spacing=spacing;

  return 0;
}
