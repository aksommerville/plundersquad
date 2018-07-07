/* ps_widget_menu2d.c
 * Substitute for ps_widget_menu, with 2-D icons instead of 1-D text.
 * We need this for the assemblepage, where the heropacker menu is too large to display plainly with more than five devices.
 * Children:
 *  [0] thumb
 *  [...] contents
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"

/* Object definition.
 */

struct ps_widget_menu2d {
  struct ps_widget hdr;
  int thumbp; // Child index, so the lowest is 1
};

#define WIDGET ((struct ps_widget_menu2d*)widget)

/* Delete.
 */

static void _ps_menu2d_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_menu2d_init(struct ps_widget *widget) {

  struct ps_widget *child;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_blotter))) return -1;
  child->bgrgba=0xc0a000ff;
  
  WIDGET->thumbp=1;
  
  return 0;
}

/* Draw.
 */

static int _ps_menu2d_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_menu2d_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 * Pack one row at a time:
 *  - Minimum one child per row, even if it's too big.
 *  - Extra horizontal space is apportioned equally.
 *  - Tallest child in the row establishes row height; children then center vetically.
 *  - This means navigating left/right is always sensible, but up/down might take some questionable diagonals.
 */

static int _ps_menu2d_pack(struct ps_widget *widget) {

  /* Walk rows... */
  int childp=1;
  int y=0;
  while (childp<widget->childc) {
    int childc=0;
    int rowh=0;
    int roww=0;
    
    /* Count children for this row... */
    while (childp+childc<widget->childc) {
      struct ps_widget *child=widget->childv[childp+childc];
      int chw,chh;
      if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
      if (!childc||(roww<=widget->w-chw)) {
        childc++;
        roww+=chw;
      } else {
        break;
      }
      if (chh>rowh) rowh=chh;
    }
    
    /* Position this row's children... */
    int xexcess=widget->w-roww;
    if (xexcess<0) xexcess=0;
    int xdumpc=childc+1; // How many spaces are there to dump excess into?
    int xexcess_each=xexcess/xdumpc;
    int xexcess_extra=xexcess%xdumpc;
    int x=0,i=0;
    for (;i<childc;i++) {
      struct ps_widget *child=widget->childv[childp+i];
      int chw,chh;
      if (ps_widget_measure(&chw,&chh,child,widget->w-x,widget->h-y)<0) return -1;
      x+=xexcess_each;
      if (xexcess_extra) {
        x++;
        xexcess_extra--;
      }
      child->w=chw;
      child->h=chh;
      child->x=x;
      child->y=y+(rowh>>1)-(chh>>1);
      x+=chw;
    }
    
    childp+=childc;
    y+=rowh;
  }
  
  /* Now that everything is placed, make another pass to center everything vertically. */
  int bottom=0,i;
  for (i=1;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int childbottom=child->y+child->h;
    if (childbottom>bottom) {
      bottom=childbottom;
    }
  }
  if (bottom<widget->h) {
    int addy=(widget->h-bottom)>>1;
    for (i=1;i<widget->childc;i++) {
      struct ps_widget *child=widget->childv[i];
      child->y+=addy;
    }
  }
  
  struct ps_widget *thumb=widget->childv[0];
  if ((WIDGET->thumbp>=1)&&(WIDGET->thumbp<widget->childc)) {
    struct ps_widget *selection=widget->childv[WIDGET->thumbp];
    thumb->x=selection->x;
    thumb->y=selection->y;
    thumb->w=selection->w;
    thumb->h=selection->h;
  } else {
    thumb->x=0;
    thumb->y=0;
    thumb->w=0;
    thumb->h=0;
  }

  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_menu2d={

  .name="menu2d",
  .objlen=sizeof(struct ps_widget_menu2d),

  .del=_ps_menu2d_del,
  .init=_ps_menu2d_init,

  .draw=_ps_menu2d_draw,
  .measure=_ps_menu2d_measure,
  .pack=_ps_menu2d_pack,

};

/* Add option.
 */
 
struct ps_widget *ps_widget_menu2d_add_option(struct ps_widget *widget,int iconid,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_menu2d)) return 0;
  struct ps_widget *icon=ps_widget_spawn(widget,&ps_widget_type_button);
  if (!icon) return 0;
  icon->bgrgba=0x00000000;
  if (ps_widget_button_set_margins(icon,0,0)<0) return 0;
  if (ps_widget_button_set_icon(icon,iconid)<0) return 0;
  if (ps_widget_button_set_callback(icon,cb)<0) return 0;
  return icon;
}

/* Activate.
 */
 
int ps_widget_menu2d_activate(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_menu2d)) return -1;
  if ((WIDGET->thumbp<1)||(WIDGET->thumbp>=widget->childc)) return 0;
  struct ps_widget *button=widget->childv[WIDGET->thumbp];
  return ps_widget_activate(button);
}

/* Move thumb.
 */
 
int ps_widget_menu2d_change_selection(struct ps_widget *widget,int dx,int dy) {
  if (!widget||(widget->type!=&ps_widget_type_menu2d)) return -1;
  
  /* No current selection. Highlight the first one, and get out. No animation. */
  if ((WIDGET->thumbp<1)||(WIDGET->thumbp>=widget->childc)) {
    WIDGET->thumbp=1;
    if (ps_widget_pack(widget)<0) return -1;
    return 0;
  }
  int nthumbp=WIDGET->thumbp;
  struct ps_widget *selection=widget->childv[nthumbp];
  
  /* Horizontal:
   *  - Vertical center must be within 1 pixel.
   *  - Take the nearest horizontally in the direction requested.
   *  - Not found? Take the furthest horizontally in the other direction.
   */
  if (dx) {
    int preferred=0,fallback=0;
    int i=1; for (;i<widget->childc;i++) {
      if (i==nthumbp) continue;
      struct ps_widget *other=widget->childv[i];
      int wdy=other->y+(other->h>>1)-selection->y-(selection->h>>1);
      if ((wdy<-1)||(wdy>1)) continue;
      if (dx<0) {
        if (other->x<selection->x) {
          if (!preferred||(other->x>widget->childv[preferred]->x)) preferred=i;
        } else {
          if (!fallback||(other->x>widget->childv[fallback]->x)) fallback=i;
        }
      } else {
        if (other->x>selection->x) {
          if (!preferred||(other->x<widget->childv[preferred]->x)) preferred=i;
        } else {
          if (!fallback||(other->x<widget->childv[fallback]->x)) fallback=i;
        }
      }
    }
    if (preferred) nthumbp=preferred;
    else if (fallback) nthumbp=fallback;
    selection=widget->childv[nthumbp];
  }
  
  /* Vertical:
   *  - Find any child in the correct row to use as a reference.
   *  - That is the next row if possible, otherwise the furthest in the other direction.
   *  - Then only examine children within one pixel of reference's vertical center.
   *  - Take the horizontally closest of these.
   */
  if (dy) {
    struct ps_widget *reference=0;
    int i=1; for (;i<widget->childc;i++) { // Find the next row...
      if (i==nthumbp) continue;
      struct ps_widget *candidate=widget->childv[i];
      if (dy<0) {
        if (candidate->y+candidate->h<=selection->y) {
          if (!reference||(candidate->y>reference->y)) reference=candidate;
        }
      } else {
        if (candidate->y>=selection->y+selection->h) {
          if (!reference||(candidate->y<reference->y)) reference=candidate;
        }
      }
    }
    if (!reference) { // No next row, find the wrap-around row...
      for (i=1;i<widget->childc;i++) {
        if (i==nthumbp) continue;
        struct ps_widget *candidate=widget->childv[i];
        if (dy<0) {
          if (!reference||(candidate->y>reference->y)) reference=candidate;
        } else {
          if (!reference||(candidate->y<reference->y)) reference=candidate;
        }
      }
    }
    if (reference) { // Could be null, eg if there is only one child.
      struct ps_widget *preferred=reference;
      int prefdx=(preferred->x+(preferred->w>>1))-(selection->x+(selection->w>>1));
      if (prefdx<0) prefdx=-prefdx;
      for (i=1;i<widget->childc;i++) {
        if (i==nthumbp) continue;
        struct ps_widget *candidate=widget->childv[i];
        int distance=(candidate->y+(candidate->h>>1))-(preferred->y+(preferred->h>>1));
        if ((distance<-1)||(distance>1)) continue;
        int canddx=(candidate->x+(candidate->w>>1))-(selection->x+(selection->w>>1));
        if (canddx<0) canddx=-canddx;
        if (canddx<prefdx) {
          preferred=candidate;
        }
      }
      for (i=1;i<widget->childc;i++) {
        if (widget->childv[i]==preferred) {
          nthumbp=i;
          selection=preferred;
          break;
        }
      }
    }
  }
  
  /* If it actually changed, commit it and animate the thumb. */
  if (nthumbp!=WIDGET->thumbp) {
    WIDGET->thumbp=nthumbp;
    struct ps_gui *gui=ps_widget_get_gui(widget);
    if (gui) {
      struct ps_widget *thumb=widget->childv[0];
      const int duration=10;
      if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_x,selection->x,duration)<0) return -1;
      if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_y,selection->y,duration)<0) return -1;
      if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_w,selection->w,duration)<0) return -1;
      if (ps_gui_animate_property(gui,thumb,PS_WIDGET_PROPERTY_h,selection->h,duration)<0) return -1;
    }
  }
   
  return 0;
}
