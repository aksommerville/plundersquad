/* ps_widget_editgrpmask.c
 * Modal dialogue for editing a 32-bit sprite group mask.
 *
 * Children:
 *  [0..31] bit toggles
 *  [32] okbutton
 *  [33] cancelbutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"

#define PS_EDITGRPMASK_MARGIN 5
#define PS_EDITGRPMASK_ROWC   4
#define PS_EDITGRPMASK_COLC   8
#define PS_EDITGRPMASK_BUTTON_HEIGHT 16

static int ps_editgrpmask_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_editgrpmask_cb_cancel(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editgrpmask {
  struct ps_widget hdr;
  uint32_t grpmask;
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_editgrpmask*)widget)

/* Delete.
 */

static void _ps_editgrpmask_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_editgrpmask_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;

  int i; for (i=0;i<32;i++) {
    struct ps_widget *toggle=ps_widget_spawn(widget,&ps_widget_type_toggle);
    if (!toggle) return -1;
    if (ps_widget_toggle_set_icon(toggle,0x0140+i)<0) return -1;
  }

  if (!ps_widget_button_spawn(widget,0,"OK",2,ps_callback(ps_editgrpmask_cb_ok,0,widget))) return -1;
  if (!ps_widget_button_spawn(widget,0,"Cancel",6,ps_callback(ps_editgrpmask_cb_cancel,0,widget))) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_editgrpmask_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editgrpmask) return -1;
  if (widget->childc!=34) return -1;
  return 0;
}

static struct ps_widget *ps_editgrpmask_get_toggle(const struct ps_widget *widget,int index) {
  if ((index<0)||(index>31)) return 0;
  return widget->childv[index];
}

static struct ps_widget *ps_editgrpmask_get_okbutton(const struct ps_widget *widget) { return widget->childv[32]; }
static struct ps_widget *ps_editgrpmask_get_cancelbutton(const struct ps_widget *widget) { return widget->childv[33]; }

/* Measure.
 */

static int _ps_editgrpmask_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=(PS_EDITGRPMASK_MARGIN<<1)+(PS_TILESIZE*PS_EDITGRPMASK_COLC);
  *h=(PS_EDITGRPMASK_MARGIN*3)+(PS_TILESIZE*PS_EDITGRPMASK_ROWC)+PS_EDITGRPMASK_BUTTON_HEIGHT;
  return 0;
}

/* Pack.
 */

static int _ps_editgrpmask_pack(struct ps_widget *widget) {
  if (ps_editgrpmask_obj_validate(widget)<0) return -1;
  struct ps_widget *okbutton=ps_editgrpmask_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_editgrpmask_get_cancelbutton(widget);
  int i,chw,chh;

  for (i=0;i<32;i++) {
    struct ps_widget *toggle=ps_editgrpmask_get_toggle(widget,i);
    if (!toggle) return -1;
    toggle->x=PS_EDITGRPMASK_MARGIN+(i%PS_EDITGRPMASK_COLC)*PS_TILESIZE;
    toggle->y=PS_EDITGRPMASK_MARGIN+(i/PS_EDITGRPMASK_COLC)*PS_TILESIZE;
    toggle->w=PS_TILESIZE;
    toggle->h=PS_TILESIZE;
  }

  if (ps_widget_measure(&chw,&chh,okbutton,widget->w,PS_EDITGRPMASK_BUTTON_HEIGHT)<0) return -1;
  okbutton->x=widget->w-PS_EDITGRPMASK_MARGIN-chw;
  okbutton->y=widget->h-PS_EDITGRPMASK_MARGIN-PS_EDITGRPMASK_BUTTON_HEIGHT;
  okbutton->w=chw;
  okbutton->h=PS_EDITGRPMASK_BUTTON_HEIGHT;
  if (ps_widget_measure(&chw,&chh,cancelbutton,widget->w,PS_EDITGRPMASK_BUTTON_HEIGHT)<0) return -1;
  cancelbutton->x=okbutton->x-chw;
  cancelbutton->y=okbutton->y;
  cancelbutton->w=chw;
  cancelbutton->h=okbutton->h;

  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editgrpmask={

  .name="editgrpmask",
  .objlen=sizeof(struct ps_widget_editgrpmask),

  .del=_ps_editgrpmask_del,
  .init=_ps_editgrpmask_init,

  .measure=_ps_editgrpmask_measure,
  .pack=_ps_editgrpmask_pack,

};

/* Callbacks.
 */

static int ps_editgrpmask_cb_ok(struct ps_widget *button,struct ps_widget *widget) {

  // Update value from toggles.
  WIDGET->grpmask=0;
  int i=0; for (;i<32;i++) {
    struct ps_widget *toggle=ps_editgrpmask_get_toggle(widget,i);
    if (!toggle) return -1;
    if (ps_widget_toggle_get_value(toggle)) {
      WIDGET->grpmask|=(1<<i);
    }
  }

  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editgrpmask_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Accessors.
 */
 
int ps_widget_editgrpmask_set_grpmask(struct ps_widget *widget,uint32_t grpmask) {
  if (ps_editgrpmask_obj_validate(widget)<0) return -1;
  WIDGET->grpmask=grpmask;
  int i=0; for (;i<32;i++) {
    struct ps_widget *toggle=ps_editgrpmask_get_toggle(widget,i);
    if (!toggle) return -1;
    if (ps_widget_toggle_set_value(toggle,grpmask&(1<<i))<0) return -1;
  }
  return 0;
}

uint32_t ps_widget_editgrpmask_get_grpmask(const struct ps_widget *widget) {
  if (ps_editgrpmask_obj_validate(widget)<0) return 0;
  return WIDGET->grpmask;
}

int ps_widget_editgrpmask_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (ps_editgrpmask_obj_validate(widget)<0) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}
