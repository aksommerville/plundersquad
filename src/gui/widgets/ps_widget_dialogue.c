#include "ps.h"
#include "gui/ps_gui.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define PS_DIALOGUE_MARGIN 10
#define PS_DIALOGUE_BUTTON_SPACING 5

/* Widget definition.
 */

struct ps_widget_dialogue {
  struct ps_widget hdr;
  struct ps_widget *message;
  struct ps_widget *input;
};

#define WIDGET ((struct ps_widget_dialogue*)widget)

/* Delete.
 */

static void _ps_dialogue_del(struct ps_widget *widget) {
  ps_widget_del(WIDGET->message);
  ps_widget_del(WIDGET->input);
}

/* Initialize.
 */

static int _ps_dialogue_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;

  return 0;
}

/* Draw frame.
 */

static int ps_dialogue_draw_frame(struct ps_widget *widget,int x0,int y0) {
  struct akgl_vtx_raw vtxv[5]={
    {x0+widget->x,y0+widget->y,0x00,0x00,0x00,0xff},
    {x0+widget->x+widget->w,y0+widget->y,0x00,0x00,0x00,0xff},
    {x0+widget->x+widget->w,y0+widget->y+widget->h,0x00,0x00,0x00,0xff},
    {x0+widget->x,y0+widget->y+widget->h,0x00,0x00,0x00,0xff},
    {x0+widget->x,y0+widget->y,0x00,0x00,0x00,0xff},
  };
  if (ps_video_draw_line_strip(vtxv,5)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_dialogue_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  if (ps_dialogue_draw_frame(widget,x0,y0)<0) return -1;
  return 0;
}

/* Is this child widget a button?
 * ie is it not the message or input?
 */

static int ps_dialogue_widget_is_button(struct ps_widget *widget,struct ps_widget *child) {
  if (child==WIDGET->message) return 0;
  if (child==WIDGET->input) return 0;
  return 1;
}

/* Measure.
 */

static int _ps_dialogue_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  int chw,chh,i;
  *w=0;
  *h=0;

  if (WIDGET->message) {
    if (ps_widget_measure(&chw,&chh,WIDGET->message,maxw,maxh)<0) return -1;
    if (chw>*w) *w=chw;
    (*h)+=chh;
  }

  if (WIDGET->input) {
    if (ps_widget_measure(&chw,&chh,WIDGET->input,maxw,maxh)<0) return -1;
    if (chw>*w) *w=chw;
    (*h)+=chh;
  }

  int btnw=0,btnh=0,btnc=0;
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (!ps_dialogue_widget_is_button(widget,child)) continue;
    if (ps_widget_measure(&chw,&chh,child,maxw,maxh)<0) return -1;
    btnw+=chw;
    if (chh>btnh) btnh=chh;
    btnc++;
  }
  if (btnc>1) btnw+=(btnc-1)*PS_DIALOGUE_BUTTON_SPACING;
  if (btnw>*w) *w=btnw;
  (*h)+=btnh;
  
  (*w)+=PS_DIALOGUE_MARGIN<<1;
  (*h)+=PS_DIALOGUE_MARGIN<<1;
  return 0;
}

/* Pack.
 */

static int _ps_dialogue_pack(struct ps_widget *widget) {
  int x=PS_DIALOGUE_MARGIN;
  int y=PS_DIALOGUE_MARGIN;
  int chw,chh,i;

  if (WIDGET->message) {
    if (ps_widget_measure(&chw,&chh,WIDGET->message,widget->w,widget->h)<0) return -1;
    WIDGET->message->x=x;
    WIDGET->message->y=y;
    WIDGET->message->w=widget->w-(PS_DIALOGUE_MARGIN<<1);
    WIDGET->message->h=chh;
    if (ps_widget_pack(WIDGET->message)<0) return -1;
    y+=WIDGET->message->h;
  }

  if (WIDGET->input) {
    if (ps_widget_measure(&chw,&chh,WIDGET->input,widget->w,widget->h)<0) return -1;
    WIDGET->input->x=x;
    WIDGET->input->y=y;
    WIDGET->input->w=widget->w-(PS_DIALOGUE_MARGIN<<1);
    WIDGET->input->h=chh;
    if (ps_widget_pack(WIDGET->input)<0) return -1;
    y+=WIDGET->input->h;
  }

  int btnh=0;
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (!ps_dialogue_widget_is_button(widget,child)) continue;
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
    if (chh>btnh) btnh=chh;
    child->x=x;
    child->y=y;
    child->w=chw;
    child->h=chh;
    if (ps_widget_pack(child)<0) return -1;
    x+=child->w;
    x+=PS_DIALOGUE_BUTTON_SPACING;
  }
  
  return 0;
}

/* Mouse events.
 */

static int _ps_dialogue_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_dialogue_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_dialogue_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_dialogue_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_dialogue_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_dialogue_mousemove(struct ps_widget *widget,int x,int y) {
  return 0;
}

/* Unified events.
 */

static int _ps_dialogue_activate(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_dialogue={
  .name="dialogue",
  .objlen=sizeof(struct ps_widget_dialogue),
  .del=_ps_dialogue_del,
  .init=_ps_dialogue_init,
  .draw=_ps_dialogue_draw,
  .measure=_ps_dialogue_measure,
  .pack=_ps_dialogue_pack,
  .mouseenter=_ps_dialogue_mouseenter,
  .mouseexit=_ps_dialogue_mouseexit,
  .mousedown=_ps_dialogue_mousedown,
  .mouseup=_ps_dialogue_mouseup,
  .mousewheel=_ps_dialogue_mousewheel,
  .mousemove=_ps_dialogue_mousemove,
  .activate=_ps_dialogue_activate,
};

/* Set message.
 */
 
int ps_widget_dialogue_set_message(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return -1;

  if (!WIDGET->message) {
    if (!(WIDGET->message=ps_widget_new(&ps_widget_type_label))) return -1;
    if (ps_widget_add_child(widget,WIDGET->message)<0) return -1;
    WIDGET->message->fgrgba=0x000000ff;
  }

  if (ps_widget_label_set_text(WIDGET->message,src,srcc)<0) return -1;
  
  return 0;
}

/* Set default text.
 */
 
int ps_widget_dialogue_set_default(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return -1;

  if (!WIDGET->input) {
    if (!(WIDGET->input=ps_widget_new(&ps_widget_type_label))) return -1;//TODO text entry widget
    if (ps_widget_add_child(widget,WIDGET->input)<0) return -1;
    WIDGET->input->fgrgba=0x000080ff;
  }

  if (ps_widget_label_set_text(WIDGET->input,src,srcc)<0) return -1;
  
  return 0;
}

/* Add button.
 */
 
struct ps_widget *ps_widget_dialogue_add_button(
  struct ps_widget *widget,
  const char *text,int textc,
  int (*cb)(struct ps_widget *label,void *userdata),
  void *userdata,
  void (*userdata_del)(void *userdata)
) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return 0;

  struct ps_widget *button=ps_widget_spawn_label(widget,text,textc,0x004000ff);
  if (!button) return 0;

  if (ps_widget_label_set_click_cb(button,cb,userdata,userdata_del)<0) return 0;
  
  return button;
}
