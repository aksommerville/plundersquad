/* ps_widget_menubar.c
 * Generic horiztonal ribbon of text buttons with user-provided callbacks.
 */

#include "ps.h"
#include "gui/ps_gui.h"

#define PS_MENUBAR_SPACING 5
#define PS_MENUBAR_BUTTON_COLOR   0x80808000
#define PS_MENUBAR_TEXT_COLOR     0x000000ff

/* Widget definition.
 */

struct ps_widget_menubar {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_menubar*)widget)

/* Delete.
 */

static void _ps_menubar_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_menubar_init(struct ps_widget *widget) {
  return 0;
}

/* Draw.
 */

static int _ps_menubar_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_menubar_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_MENUBAR_SPACING;
  *h=0;
  int i=widget->childc; while (i-->0) {
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,widget->childv[i],maxw,maxh)<0) return -1;
    chw+=PS_MENUBAR_SPACING;
    (*w)+=chw;
    maxw-=chw;
    if (chh>*h) *h=chh;
  }
  return 0;
}

/* Pack.
 */

static int _ps_menubar_pack(struct ps_widget *widget) {
  int x=PS_MENUBAR_SPACING,i=0;
  for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int chw,chh;
    if (ps_widget_measure(&chw,&chh,child,widget->w-x,widget->h)<0) return -1;
    child->x=x;
    child->y=0;
    child->w=chw;
    child->h=widget->h;
    if (ps_widget_pack(child)<0) return -1;
    x+=chw;
    x+=PS_MENUBAR_SPACING;
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
  .draw=_ps_menubar_draw,
  .measure=_ps_menubar_measure,
  .pack=_ps_menubar_pack,
};

/* Add item.
 */
 
struct ps_widget *ps_widget_menubar_add_menu(
  struct ps_widget *widget,
  const char *text,int textc,
  int (*cb)(struct ps_widget *label,void *userdata),
  void *userdata,
  void (*userdata_del)(void *userdata)
) {
  if (!widget||(widget->type!=&ps_widget_type_menubar)) return 0;
  struct ps_widget *child=ps_widget_spawn_label(widget,text,textc,PS_MENUBAR_TEXT_COLOR);
  if (ps_widget_label_set_click_cb(child,cb,userdata,userdata_del)<0) return 0;
  child->bgrgba=PS_MENUBAR_BUTTON_COLOR;
  return child;
}
