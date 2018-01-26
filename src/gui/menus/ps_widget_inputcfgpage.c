/* ps_widget_inputcfgpage.c
 * Modal full-screen page for input configuration.
 * This can be launched from the setup or pause menu.
 * We work quite a lot like the assemble page.
 * Children:
 *  [0] title
 *  [1] inputcfgpacker
 *  [2] okbutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"

static int ps_inputcfgpage_cb_done(struct ps_widget *button,struct ps_widget *widget);
static int ps_inputcfgpage_reset_device(struct ps_widget *widget,int childp);

/* Object definition.
 */

struct ps_widget_inputcfgpage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_inputcfgpage*)widget)

/* Delete.
 */

static void _ps_inputcfgpage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_inputcfgpage_init(struct ps_widget *widget) {

  widget->bgrgba=0x804020ff;

  struct ps_widget *child;
  if (!(child=ps_widget_label_spawn(widget,"Input Configuration",-1))) return -1;
  child->fgrgba=0xffffffff;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_inputcfgpacker))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Done [F1]",-1,ps_callback(ps_inputcfgpage_cb_done,0,widget)))) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_inputcfgpage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_inputcfgpage) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_inputcfgpage_get_title(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_inputcfgpage_get_inputcfgpacker(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_inputcfgpage_get_okbutton(const struct ps_widget *widget) { return widget->childv[2]; }

/* Measure.
 */

static int _ps_inputcfgpage_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SCREENW;
  *h=PS_SCREENH;
  return 0;
}

/* Pack.
 */

static int _ps_inputcfgpage_pack(struct ps_widget *widget) {
  if (ps_inputcfgpage_obj_validate(widget)<0) return -1;
  struct ps_widget *title=ps_inputcfgpage_get_title(widget);
  struct ps_widget *inputcfgpacker=ps_inputcfgpage_get_inputcfgpacker(widget);
  struct ps_widget *okbutton=ps_inputcfgpage_get_okbutton(widget);
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,title,widget->w,widget->h)<0) return -1;
  title->x=0;
  title->y=0;
  title->w=widget->w;
  title->h=chh;

  if (ps_widget_measure(&chw,&chh,okbutton,widget->w,title->h)<0) return -1;
  okbutton->x=widget->w-chw;
  okbutton->y=0;
  okbutton->w=chw;
  okbutton->h=title->h;

  inputcfgpacker->x=0;
  inputcfgpacker->y=title->y+title->h;
  inputcfgpacker->w=widget->w;
  inputcfgpacker->h=widget->h-inputcfgpacker->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Receive input via GUI.
 * The rest of the GUI doesn't use 'F' keys, and I think it's unlikely that a player would map those to his controller.
 * So we use 'F' keys heavily for out-of-band stuff.
 * Interface is a little goofy because hey, we are the input configurator.
 */

//TODO There must be some way to manage inputcfgpage without keyboard or mouse.

static int _ps_inputcfgpage_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  //ps_log(GUI,TRACE,"%s %d[U+%x]=%d",__func__,keycode,codepoint,value);
  if (value==1) switch (keycode) {

    // F1: Done.
    case 0x0007003a: return ps_widget_kill(widget);

    // F2..F12: Reset device.
    case 0x0007003b: return ps_inputcfgpage_reset_device(widget,0);
    case 0x0007003c: return ps_inputcfgpage_reset_device(widget,1);
    case 0x0007003d: return ps_inputcfgpage_reset_device(widget,2);
    case 0x0007003e: return ps_inputcfgpage_reset_device(widget,3);
    case 0x0007003f: return ps_inputcfgpage_reset_device(widget,4);
    case 0x00070040: return ps_inputcfgpage_reset_device(widget,5);
    case 0x00070041: return ps_inputcfgpage_reset_device(widget,6);
    case 0x00070042: return ps_inputcfgpage_reset_device(widget,7);
    case 0x00070043: return ps_inputcfgpage_reset_device(widget,8);
    case 0x00070044: return ps_inputcfgpage_reset_device(widget,9);
    case 0x00070045: return ps_inputcfgpage_reset_device(widget,10);
    
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_inputcfgpage={

  .name="inputcfgpage",
  .objlen=sizeof(struct ps_widget_inputcfgpage),

  .del=_ps_inputcfgpage_del,
  .init=_ps_inputcfgpage_init,

  .measure=_ps_inputcfgpage_measure,
  .pack=_ps_inputcfgpage_pack,

  .key=_ps_inputcfgpage_key,

};

/* "Done" button.
 */
 
static int ps_inputcfgpage_cb_done(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Reset child, in response to F2..F12
 */
 
static int ps_inputcfgpage_reset_device(struct ps_widget *widget,int childp) {
  struct ps_widget *inputcfgpacker=ps_inputcfgpage_get_inputcfgpacker(widget);
  if ((childp<0)||(childp>=inputcfgpacker->childc)) {
    ps_log(GUI,WARN,"No device #%d, discarding request to reset.",childp);
    return 0;
  }
  struct ps_widget *inputcfg=inputcfgpacker->childv[childp];
  if (ps_inputcfg_cb_begin_reset(0,inputcfg)<0) return -1;
  return 0;
}
