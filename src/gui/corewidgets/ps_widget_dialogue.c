/* ps_widget_dialogue.c
 * We have one child, a vertical packer.
 * That packer has up to three children, in order:
 *   - optional textblock
 *   - optional field
 *   - mandatory horizontal packer for buttons
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "util/ps_geometry.h"
#include "util/ps_text.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* Object definition.
 */

struct ps_widget_dialogue {
  struct ps_widget hdr;
  void *userdata;
  void (*userdata_del)(void *userdata);
  int refnum1;
};

#define WIDGET ((struct ps_widget_dialogue*)widget)

/* Delete.
 */

static void _ps_dialogue_del(struct ps_widget *widget) {
  if (WIDGET->userdata_del) WIDGET->userdata_del(WIDGET->userdata);
}

/* Initialize.
 */

static int _ps_dialogue_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;
  widget->fgrgba=0x000000ff;

  struct ps_widget *mainpacker=ps_widget_spawn(widget,&ps_widget_type_packer);
  if (!mainpacker) return -1;
  if (ps_widget_packer_set_axis(mainpacker,PS_AXIS_VERT)<0) return -1;
  if (ps_widget_packer_set_margins(mainpacker,5,2)<0) return -1;
  if (ps_widget_packer_set_alignment(mainpacker,PS_ALIGN_FILL,PS_ALIGN_FILL)<0) return -1;

  struct ps_widget *buttonpacker=ps_widget_spawn(mainpacker,&ps_widget_type_packer);
  if (!buttonpacker) return -1;
  if (ps_widget_packer_set_axis(buttonpacker,PS_AXIS_HORZ)<0) return -1;
  if (ps_widget_packer_set_margins(buttonpacker,0,2)<0) return -1;
  if (ps_widget_packer_set_alignment(buttonpacker,PS_ALIGN_END,PS_ALIGN_FILL)<0) return -1;

  return 0;
}

/* Draw frame.
 */

static int ps_dialogue_draw_frame(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  uint8_t r=widget->fgrgba>>24;
  uint8_t g=widget->fgrgba>>16;
  uint8_t b=widget->fgrgba>>8;
  uint8_t a=widget->fgrgba;
  struct akgl_vtx_raw vtxv[5]={
    {parentx,parenty,r,g,b,a},
    {parentx+widget->w,parenty,r,g,b,a},
    {parentx+widget->w,parenty+widget->h,r,g,b,a},
    {parentx,parenty+widget->h,r,g,b,a},
    {parentx,parenty,r,g,b,a},
  };
  if (ps_video_draw_line_strip(vtxv,5)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_dialogue_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  if (ps_dialogue_draw_frame(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_dialogue_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (widget->childc!=1) return -1;

  /* Boundary limits should be the full screen.
   * If not, give up and let the packer decide.
   */
  if ((maxw!=PS_SCREENW)||(maxh!=PS_SCREENH)) {
    return ps_widget_measure(w,h,widget->childv[0],maxw,maxh);
  }

  /* Measure at half the screen's width provisionally.
   */
  if (ps_widget_measure(w,h,widget->childv[0],maxw>>1,maxh)<0) return -1;

  /* If the height ended up greater than half, try 2/3 instead.
   */
  if (*h>PS_SCREENH>>1) {
    if (ps_widget_measure(w,h,widget->childv[0],(maxw*2)/3,maxh)<0) return -1;
    // And if that leaves the height taller than the screen, go full size.
    if (*h>=PS_SCREENH) {
      if (ps_widget_measure(w,h,widget->childv[0],maxw,maxh)<0) return -1;
    }
  }

  return 0;
}

/* Pack.
 */

static int _ps_dialogue_pack(struct ps_widget *widget) {
  if (widget->childc!=1) return -1;
  struct ps_widget *mainpacker=widget->childv[0];
  mainpacker->x=0;
  mainpacker->y=0;
  mainpacker->w=widget->w;
  mainpacker->h=widget->h;
  if (ps_widget_pack(mainpacker)<0) return -1;
  return 0;
}

/* Primitive input events.
 */

static int _ps_dialogue_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_dialogue_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_dialogue_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_dialogue_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_dialogue_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_dialogue_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_dialogue_activate(struct ps_widget *widget) {
  ps_log(GUI,INFO,"TODO: %s",__func__);
  return 0;
}

static int _ps_dialogue_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_dialogue_adjust(struct ps_widget *widget,int d) {
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

  .mousemotion=_ps_dialogue_mousemotion,
  .mousebutton=_ps_dialogue_mousebutton,
  .mousewheel=_ps_dialogue_mousewheel,
  .userinput=_ps_dialogue_userinput,

  .mouseenter=_ps_dialogue_mouseenter,
  .mouseexit=_ps_dialogue_mouseexit,
  .activate=_ps_dialogue_activate,
  .cancel=_ps_dialogue_cancel,
  .adjust=_ps_dialogue_adjust,

};

/* Get specific descendant widgets.
 * 'message' is always first or absent, type textblock.
 * 'dialogue' is always second to last or absent, type field.
 * 'buttonpacker' is always last and always present, type packer.
 */

static struct ps_widget *ps_dialogue_get_message(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)||(widget->childc!=1)) return 0;
  const struct ps_widget *mainpacker=widget->childv[0];
  if (mainpacker->childc<2) return 0;
  struct ps_widget *message=mainpacker->childv[0];
  if (message->type!=&ps_widget_type_textblock) return 0;
  return message;
}

static struct ps_widget *ps_dialogue_get_input(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)||(widget->childc!=1)) return 0;
  const struct ps_widget *mainpacker=widget->childv[0];
  if (mainpacker->childc<2) return 0;
  struct ps_widget *input=mainpacker->childv[mainpacker->childc-2];
  if (input->type!=&ps_widget_type_field) return 0;
  return input;
}

static struct ps_widget *ps_dialogue_get_buttonpacker(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)||(widget->childc!=1)) return 0;
  const struct ps_widget *mainpacker=widget->childv[0];
  if (mainpacker->childc<1) return 0;
  struct ps_widget *buttonpacker=mainpacker->childv[mainpacker->childc-1];
  if (buttonpacker->type!=&ps_widget_type_packer) return 0;
  return buttonpacker;
}

/* Create children.
 */

static struct ps_widget *ps_dialogue_create_message(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)||(widget->childc!=1)) return 0;
  struct ps_widget *mainpacker=widget->childv[0];
  struct ps_widget *message=ps_widget_new(&ps_widget_type_textblock);
  if (!message) return 0;
  if (ps_widget_insert_child(mainpacker,0,message)<0) {
    ps_widget_del(message);
    return 0;
  }
  ps_widget_del(message);
  return message;
}

static struct ps_widget *ps_dialogue_create_input(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)||(widget->childc!=1)) return 0;
  struct ps_widget *mainpacker=widget->childv[0];
  struct ps_widget *input=ps_widget_new(&ps_widget_type_field);
  if (!input) return 0;
  if (ps_widget_insert_child(mainpacker,mainpacker->childc-1,input)<0) {
    ps_widget_del(input);
    return 0;
  }
  ps_widget_del(input);
  return input;
}

/* Set message.
 */
 
struct ps_widget *ps_widget_dialogue_set_message(struct ps_widget *widget,const char *text,int textc) {
  struct ps_widget *message=ps_dialogue_get_message(widget);
  if (!message) {
    if (!(message=ps_dialogue_create_message(widget))) return 0;
  }
  if (ps_widget_textblock_set_text(message,text,textc)<0) return 0;
  return message;
}

/* Set default input, and initialize input field if needed.
 */
 
struct ps_widget *ps_widget_dialogue_set_input(struct ps_widget *widget,const char *text,int textc) {
  struct ps_widget *input=ps_dialogue_get_input(widget);
  if (!input) {
    if (!(input=ps_dialogue_create_input(widget))) return 0;
  }
  if (ps_widget_field_set_text(input,text,textc)<0) return 0;
  return input;
}

/* Add a button.
 */
 
struct ps_widget *ps_widget_dialogue_add_button(struct ps_widget *widget,const char *text,int textc,struct ps_callback cb) {
  struct ps_widget *buttonpacker=ps_dialogue_get_buttonpacker(widget);
  if (!buttonpacker) return 0;
  struct ps_widget *button=ps_widget_button_spawn(buttonpacker,0,text,textc,cb);
  if (!button) return 0;
  return button;
}

/* Get input text.
 */

int ps_widget_dialogue_get_text(void *dstpp,const struct ps_widget *widget) {
  const struct ps_widget *input=ps_dialogue_get_input(widget);
  return ps_widget_field_get_text(dstpp,input);
}

int ps_widget_dialogue_get_number(int *dst,const struct ps_widget *widget) {
  const struct ps_widget *input=ps_dialogue_get_input(widget);
  const char *text=0;
  int textc=ps_widget_field_get_text(&text,input);
  if (textc<0) return -1;
  return ps_int_eval(dst,text,textc);
}

/* Extra user data.
 */
 
int ps_widget_dialogue_set_userdata(struct ps_widget *widget,void *userdata,void (*userdata_del)(void *userdata)) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return -1;
  if (WIDGET->userdata_del) WIDGET->userdata_del(WIDGET->userdata);
  WIDGET->userdata=userdata;
  WIDGET->userdata_del=userdata_del;
  return 0;
}

void *ps_widget_dialogue_get_userdata(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return 0;
  return WIDGET->userdata;
}

int ps_widget_dialogue_set_refnum1(struct ps_widget *widget,int v) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return -1;
  WIDGET->refnum1=v;
  return 0;
}

int ps_widget_dialogue_get_refnum1(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_dialogue)) return 0;
  return WIDGET->refnum1;
}

/* Convenience ctors.
 */

static int ps_dialogue_cb_cancel(struct ps_widget *button,struct ps_widget *dialogue) {
  return ps_widget_kill(dialogue);
}
 
struct ps_widget *ps_widget_spawn_dialogue_message(
  struct ps_widget *parent,
  const char *message,int messagec,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
) {
  struct ps_widget *widget=ps_widget_spawn(parent,&ps_widget_type_dialogue);
  if (!widget) return 0;
  if (!ps_widget_dialogue_set_message(widget,message,messagec)) return 0;
  if (!ps_widget_dialogue_add_button(widget,"Cancel",6,ps_callback(ps_dialogue_cb_cancel,0,widget))) return 0;
  if (!ps_widget_dialogue_add_button(widget,"OK",2,ps_callback(cb,0,widget))) return 0;
  if (ps_widget_pack(parent)<0) return 0;
  return widget;
}

struct ps_widget *ps_widget_spawn_dialogue_text(
  struct ps_widget *parent,
  const char *message,int messagec,
  const char *input,int inputc,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
) {
  struct ps_widget *widget=ps_widget_spawn(parent,&ps_widget_type_dialogue);
  if (!widget) return 0;
  if (!ps_widget_dialogue_set_message(widget,message,messagec)) return 0;
  if (!ps_widget_dialogue_set_input(widget,input,inputc)) return 0;
  if (!ps_widget_dialogue_add_button(widget,"Cancel",6,ps_callback(ps_dialogue_cb_cancel,0,widget))) return 0;
  if (!ps_widget_dialogue_add_button(widget,"OK",2,ps_callback(cb,0,widget))) return 0;
  if (ps_widget_pack(parent)<0) return 0;
  return widget;
}

struct ps_widget *ps_widget_spawn_dialogue_number(
  struct ps_widget *parent,
  const char *message,int messagec,
  int input,
  int (*cb)(struct ps_widget *button,struct ps_widget *dialogue)
) {
  char text[32];
  int textc=ps_decsint_repr(text,sizeof(text),input);
  if ((textc<0)||(textc>sizeof(text))) return 0;
  struct ps_widget *widget=ps_widget_spawn(parent,&ps_widget_type_dialogue);
  if (!widget) return 0;
  if (!ps_widget_dialogue_set_message(widget,message,messagec)) return 0;
  if (!ps_widget_dialogue_set_input(widget,text,textc)) return 0;
  if (!ps_widget_dialogue_add_button(widget,"Cancel",6,ps_callback(ps_dialogue_cb_cancel,0,widget))) return 0;
  if (!ps_widget_dialogue_add_button(widget,"OK",2,ps_callback(cb,0,widget))) return 0;
  if (ps_widget_pack(parent)<0) return 0;
  return widget;
}
