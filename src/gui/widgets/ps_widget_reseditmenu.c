/* ps_widget_reseditmenu.c
 * Menu bar for resedit.
 * Children:
 *   [0] Back button.
 *   [1] Resource ID label.
 *   [...] Menu headers.
 */

#define PS_RESEDITMENU_SPACING 5
#define PS_RESEDITMENU_BACKGROUND_COLOR    0xe0e0e0ff
#define PS_RESEDITMENU_BUTTON_COLOR        0x80ffa0ff
#define PS_RESEDITMENU_MENU_COLOR          0x000000ff
#define PS_RESEDITMENU_LABEL_COLOR_VALID   0x808080ff
#define PS_RESEDITMENU_LABEL_COLOR_INVALID 0x800000ff
#define PS_RESEDITMENU_LABEL_TEXT_INVALID "(not loaded)"

#include "ps.h"
#include "gui/ps_gui.h"

/* Widget definition.
 */

struct ps_widget_reseditmenu {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_reseditmenu*)widget)

/* Menu item callbacks.
 */
 
static int ps_reseditmenu_cb_back(struct ps_widget *label,void *userdata) {
  if (!label->parent) return -1; // Hint that we might be dealing with a zombie, and the menu might not exist anymore.
  struct ps_widget *widget=userdata;
  if (!widget||(widget->type!=&ps_widget_type_reseditmenu)) return -1;
  if (ps_gui_load_page_edithome(ps_widget_get_gui(widget))<0) return -1;
  return 0;
}

/* Delete.
 */

static void _ps_reseditmenu_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_reseditmenu_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=PS_RESEDITMENU_BACKGROUND_COLOR;

  if (!(child=ps_widget_spawn_label(widget,"<",1,PS_RESEDITMENU_MENU_COLOR))) return -1;
  if (ps_widget_label_set_click_cb(child,ps_reseditmenu_cb_back,widget,0)<0) return -1;
  child->bgrgba=PS_RESEDITMENU_BUTTON_COLOR;
  
  if (!(child=ps_widget_spawn_label(widget,PS_RESEDITMENU_LABEL_TEXT_INVALID,-1,PS_RESEDITMENU_LABEL_COLOR_INVALID))) return -1;

  return 0;
}

/* Draw.
 */

static int _ps_reseditmenu_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 * Doesn't matter; resedit will give us a fixed size no matter what.
 */

static int _ps_reseditmenu_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 * We pack simply left-to-right, except childv[1] goes at the end.
 */

static int ps_reseditmenu_pack_children(struct ps_widget *widget,int p,int c,int x) {
  while (c-->0) {
    int chw,chh;
    struct ps_widget *child=widget->childv[p];
    if (ps_widget_measure(&chw,&chh,child,widget->w-x,widget->h)<0) return -1;
    child->x=x;
    child->w=chw;
    child->y=0;
    child->h=widget->h;
    if (ps_widget_pack(child)<0) return -1;
    p++;
    x+=chw;
    x+=PS_RESEDITMENU_SPACING;
  }
  return x;
}

static int _ps_reseditmenu_pack(struct ps_widget *widget) {
  if (widget->childc<2) return -1;
  int x=PS_RESEDITMENU_SPACING;

  if ((x=ps_reseditmenu_pack_children(widget,0,1,x))<0) return -1;
  if ((x=ps_reseditmenu_pack_children(widget,2,widget->childc-2,x))<0) return -1;
  if ((x=ps_reseditmenu_pack_children(widget,1,1,x))<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_reseditmenu={
  .name="reseditmenu",
  .objlen=sizeof(struct ps_widget_reseditmenu),
  .del=_ps_reseditmenu_del,
  .init=_ps_reseditmenu_init,
  .draw=_ps_reseditmenu_draw,
  .measure=_ps_reseditmenu_measure,
  .pack=_ps_reseditmenu_pack,
};

/* Set resource name.
 */
 
int ps_widget_reseditmenu_set_name(struct ps_widget *widget,const char *text,int textc) {
  if (!widget||(widget->type!=&ps_widget_type_reseditmenu)) return -1;
  if (widget->childc<2) return -1;
  if (!text) textc=0; else if (textc<0) { textc=0; while (text[textc]) textc++; }
  if (textc) {
    if (ps_widget_label_set_text(widget->childv[1],text,textc)<0) return -1;
    widget->childv[1]->fgrgba=PS_RESEDITMENU_LABEL_COLOR_VALID;
  } else {
    if (ps_widget_label_set_text(widget->childv[1],PS_RESEDITMENU_LABEL_TEXT_INVALID,-1)<0) return -1;
    widget->childv[1]->fgrgba=PS_RESEDITMENU_LABEL_COLOR_INVALID;
  }
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Add menu.
 */
 
struct ps_widget *ps_widget_reseditmenu_add_menu(
  struct ps_widget *widget,
  const char *text,int textc,
  int (*cb)(struct ps_widget *label,void *userdata),
  void *userdata,
  void (*userdata_del)(void *userdata)
) {
  if (!widget||(widget->type!=&ps_widget_type_reseditmenu)) return 0;
  if (widget->childc<2) return 0;
  struct ps_widget *child=ps_widget_spawn_label(widget,text,textc,PS_RESEDITMENU_MENU_COLOR);
  if (ps_widget_label_set_click_cb(child,cb,userdata,userdata_del)<0) return 0;
  child->bgrgba=PS_RESEDITMENU_BUTTON_COLOR;
  return child;
}
