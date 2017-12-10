#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "util/ps_geometry.h"

/* Object definition.
 */

struct ps_widget_packer {
  struct ps_widget hdr;
  int axis; // PS_AXIS_HORZ,PS_AXIS_VERT
  int align_major; // PS_ALIGN_{START,CENTER,END,FILL}
  int align_minor; // PS_ALIGN_{START,CENTER,END,FILL}
  int padding; // Extra space around the border.
  int spacing; // Extra space between children.
};

#define WIDGET ((struct ps_widget_packer*)widget)

/* Delete.
 */

static void _ps_packer_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_packer_init(struct ps_widget *widget) {

  WIDGET->axis=PS_AXIS_VERT;
  WIDGET->align_major=PS_ALIGN_START;
  WIDGET->align_minor=PS_ALIGN_FILL;
  WIDGET->padding=0;
  WIDGET->spacing=0;

  return 0;
}

/* Measure.
 */

static int _ps_packer_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=*h=0;
  if (widget->childc>0) {

    maxw-=WIDGET->padding<<1;
    maxh-=WIDGET->padding<<1;
    if (WIDGET->axis==PS_AXIS_VERT) {
      maxh-=WIDGET->spacing*(widget->childc-1);
    } else {
      maxw-=WIDGET->spacing*(widget->childc-1);
    }
    
    int wmax=0,wsum=0,hmax=0,hsum=0;
    int i=0; for (;i<widget->childc;i++) {
      struct ps_widget *child=widget->childv[i];
      int chw,chh;
      if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
      if (chw>wmax) wmax=chw;
      if (chh>hmax) hmax=chh;
      wsum+=chw;
      hsum+=chh;
    }
    wsum+=WIDGET->spacing*(widget->childc-1);
    hsum+=WIDGET->spacing*(widget->childc-1);
    if (WIDGET->axis==PS_AXIS_VERT) {
      *w=wmax;
      *h=hsum;
    } else {
      *w=wsum;
      *h=hmax;
    }
  }

  (*w)+=WIDGET->padding<<1;
  (*h)+=WIDGET->padding<<1;
  return 0;
}

/* Pack.
 */

static int ps_packer_pack_children(struct ps_widget *widget) {
  int i=widget->childc; while (i-->0) {
    if (ps_widget_pack(widget->childv[i])<0) return -1;
  }
  return 0;
}

static int _ps_packer_pack(struct ps_widget *widget) {
  if (widget->childc<1) return 0;

  /* Start in the top left corner. */
  int x=WIDGET->padding;
  int y=WIDGET->padding;
  int availw=widget->w-(WIDGET->padding<<1);
  int availh=widget->h-(WIDGET->padding<<1);

  /* Pre-pack widgets as if the major align is START. */
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,availw,availh)<0) return -1;

    if (WIDGET->axis==PS_AXIS_VERT) {
      child->y=y;
      child->h=chh;
      if (chw<availw) switch (WIDGET->align_minor) {
        case PS_ALIGN_START: child->x=x; child->w=chw; break;
        case PS_ALIGN_CENTER: child->x=x+(availw>>1)-(chw>>1); child->w=chw; break;
        case PS_ALIGN_END: child->x=x+availw-chw; child->w=chw; break;
        case PS_ALIGN_FILL: child->x=x; child->w=availw; break;
      } else {
        child->x=x;
        child->w=availw;
      }
      y+=chh+WIDGET->spacing;
      availh-=chh+WIDGET->spacing;

    } else {
      child->x=x;
      child->w=chw;
      if (chh<availh) switch (WIDGET->align_minor) {
        case PS_ALIGN_START: child->y=y; child->h=chh; break;
        case PS_ALIGN_CENTER: child->y=y+(availh>>1)-(chh>>1); child->h=chh; break;
        case PS_ALIGN_END: child->y=y+availh-chh; child->h=chh; break;
        case PS_ALIGN_FILL: child->y=y; child->h=availh; break;
      } else {
        child->y=y;
        child->h=availh;
      }
      x+=chw+WIDGET->spacing;
      availw-=chw+WIDGET->spacing;
    }
  }

  /* If major align is START, or we've filled the available space, we're done. */
  if (WIDGET->align_major==PS_ALIGN_START) return ps_packer_pack_children(widget);
  if (WIDGET->axis==PS_AXIS_VERT) {
    if (availh<1) return ps_packer_pack_children(widget);
  } else {
    if (availw<1) return ps_packer_pack_children(widget);
  }

  /* If major align is FILL, grow all children by the same amount (+-1). */
  if (WIDGET->align_major==PS_ALIGN_FILL) {
    if (WIDGET->axis==PS_AXIS_VERT) {
      int addall=availh/widget->childc;
      int extra=availh%widget->childc;
      int yoffset=0;
      for (i=0;i<widget->childc;) {
        struct ps_widget *child=widget->childv[i];
        child->y+=yoffset;
        yoffset+=addall;
        child->h+=addall;
        if (extra-->0) {
          child->h++;
          yoffset++;
        }
      }
    } else {
      int addall=availw/widget->childc;
      int extra=availw%widget->childc;
      int xoffset=0;
      for (i=0;i<widget->childc;) {
        struct ps_widget *child=widget->childv[i];
        child->x+=xoffset;
        xoffset+=addall;
        child->w+=addall;
        if (extra-->0) {
          child->w++;
          xoffset++;
        }
      }
    }
    return ps_packer_pack_children(widget);
  }

  /* Major align is END or CENTER. Adjust child positions. */
  if (WIDGET->axis==PS_AXIS_VERT) {
    int offset;
    if (WIDGET->align_major==PS_ALIGN_END) offset=availh;
    else offset=availh>>1;
    for (i=widget->childc;i-->0;) widget->childv[i]->y+=offset;
  } else {
    int offset;
    if (WIDGET->align_major==PS_ALIGN_END) offset=availw;
    else offset=availw>>1;
    for (i=widget->childc;i-->0;) widget->childv[i]->x+=offset;
  }
  return ps_packer_pack_children(widget);
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

/* Setup.
 */
 
int ps_widget_packer_set_margins(struct ps_widget *widget,int padding,int spacing) {
  if (!widget||(widget->type!=&ps_widget_type_packer)) return -1;
  if (padding<0) return -1;
  if (spacing<0) return -1;
  WIDGET->padding=padding;
  WIDGET->spacing=spacing;
  return 0;
}

int ps_widget_packer_set_alignment(struct ps_widget *widget,int major,int minor) {
  if (!widget||(widget->type!=&ps_widget_type_packer)) return -1;
  switch (major) {
    case PS_ALIGN_START:
    case PS_ALIGN_CENTER:
    case PS_ALIGN_END:
    case PS_ALIGN_FILL:
      break;
    default: return -1;
  }
  switch (minor) {
    case PS_ALIGN_START:
    case PS_ALIGN_CENTER:
    case PS_ALIGN_END:
    case PS_ALIGN_FILL:
      break;
    default: return -1;
  }
  WIDGET->align_major=major;
  WIDGET->align_minor=minor;
  return 0;
}

int ps_widget_packer_set_axis(struct ps_widget *widget,int axis) {
  if (!widget||(widget->type!=&ps_widget_type_packer)) return -1;
  switch (axis) {
    case PS_AXIS_VERT:
    case PS_AXIS_HORZ:
      break;
    default: return -1;
  }
  WIDGET->axis=axis;
  return 0;
}
