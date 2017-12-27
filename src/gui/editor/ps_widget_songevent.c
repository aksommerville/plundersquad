/* ps_widget_songevent.c
 * Dialogue box for editing any event from a SEM.
 * This is *not* a child of editsong; it is a top-level modal dialogue.
 *
 * Children:
 *  [0] objidfield
 *  [1] trimfield
 *  [2] panfield
 *  [3] objidlabel
 *  [4] trimlabel
 *  [5] panlabel
 *  [6] infolabel
 *  [7] okbutton
 *  [8] cancelbutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "resedit/ps_sem.h"
#include "akau/akau.h"
#include "util/ps_text.h"

#define PS_SONGEVENT_MARGIN 5

static int ps_songevent_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_songevent_cb_cancel(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_songevent {
  struct ps_widget hdr;
  struct ps_sem *sem;
  int beatp,eventp;
};

#define WIDGET ((struct ps_widget_songevent*)widget)

/* Delete.
 */

static void _ps_songevent_del(struct ps_widget *widget) {
  ps_sem_del(WIDGET->sem);
}

/* Initialize.
 */

static int _ps_songevent_init(struct ps_widget *widget) {

  widget->bgrgba=0xffffc0ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // objidfield

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // trimfield

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1; // panfield

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // objidlabel
  if (ps_widget_label_set_text(child,"objid",5)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // trimlabel
  if (ps_widget_label_set_text(child,"Trim",4)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // panlabel
  if (ps_widget_label_set_text(child,"Pan",3)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // infolabel
  if (ps_widget_label_set_text(child,"Edit event:",-1)<0) return -1;

  if (!(child=ps_widget_button_spawn(widget,0,"OK",2,ps_callback(ps_songevent_cb_ok,0,widget)))) return -1;

  if (!(child=ps_widget_button_spawn(widget,0,"Cancel",6,ps_callback(ps_songevent_cb_cancel,0,widget)))) return -1;

  return 0;
}

/* Structural helper.
 */

static int ps_songevent_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_songevent) return -1;
  if (widget->childc!=9) return -1;
  return 0;
}

static struct ps_widget *ps_songevent_get_objidfield(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_songevent_get_trimfield(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_songevent_get_panfield(const struct ps_widget *widget) {
  return widget->childv[2];
}

static struct ps_widget *ps_songevent_get_objidlabel(const struct ps_widget *widget) {
  return widget->childv[3];
}

static struct ps_widget *ps_songevent_get_trimlabel(const struct ps_widget *widget) {
  return widget->childv[4];
}

static struct ps_widget *ps_songevent_get_panlabel(const struct ps_widget *widget) {
  return widget->childv[5];
}

static struct ps_widget *ps_songevent_get_infolabel(const struct ps_widget *widget) {
  return widget->childv[6];
}

static struct ps_widget *ps_songevent_get_okbutton(const struct ps_widget *widget) {
  return widget->childv[7];
}

static struct ps_widget *ps_songevent_get_cancelbutton(const struct ps_widget *widget) {
  return widget->childv[8];
}

/* Measure.
 */

static int _ps_songevent_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SCREENW>>1;
  *h=PS_SCREENH>>1;
  return 0;
}

/* Pack.
 */

static int _ps_songevent_pack(struct ps_widget *widget) {
  if (ps_songevent_obj_validate(widget)<0) return -1;
  struct ps_widget *okbutton=ps_songevent_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_songevent_get_cancelbutton(widget);
  struct ps_widget *objidfield=ps_songevent_get_objidfield(widget);
  struct ps_widget *trimfield=ps_songevent_get_trimfield(widget);
  struct ps_widget *panfield=ps_songevent_get_panfield(widget);
  struct ps_widget *objidlabel=ps_songevent_get_objidlabel(widget);
  struct ps_widget *trimlabel=ps_songevent_get_trimlabel(widget);
  struct ps_widget *panlabel=ps_songevent_get_panlabel(widget);
  struct ps_widget *infolabel=ps_songevent_get_infolabel(widget);
  int chw,chh;

  // okbutton: bottom right
  if (ps_widget_measure(&chw,&chh,okbutton,widget->w,widget->h)<0) return -1;
  okbutton->x=widget->w-PS_SONGEVENT_MARGIN-chw;
  okbutton->y=widget->h-PS_SONGEVENT_MARGIN-chh;
  okbutton->w=chw;
  okbutton->h=chh;

  // cancelbutton: left of okbutton and same height
  if (ps_widget_measure(&chw,&chh,cancelbutton,widget->w,widget->h)<0) return -1;
  cancelbutton->x=okbutton->x-chw-2;
  cancelbutton->y=okbutton->y;
  cancelbutton->w=chw;
  cancelbutton->h=okbutton->h;

  // infolabel: top left
  if (ps_widget_measure(&chw,&chh,infolabel,widget->w,widget->h)<0) return -1;
  infolabel->x=PS_SONGEVENT_MARGIN;
  infolabel->y=PS_SONGEVENT_MARGIN;
  infolabel->w=chw;
  infolabel->h=chh;

  // Labels and fields.
  int labelright=widget->w>>1;
  int y=infolabel->y+infolabel->h;
  #define KV(tag) { \
    if (ps_widget_measure(&chw,&chh,tag##label,widget->w,widget->h)<0) return -1; \
    tag##label->x=labelright-chw; \
    tag##label->y=y; \
    tag##label->w=chw; \
    tag##label->h=chh; \
    if (ps_widget_measure(&chw,&chh,tag##field,widget->w,widget->h)<0) return -1; \
    tag##field->x=labelright+5; \
    tag##field->w=widget->w-PS_SONGEVENT_MARGIN-tag##field->x; \
    tag##field->y=y; \
    tag##field->h=chh; \
    if (tag##label->h>tag##field->h) { \
      tag##field->h=tag##label->h; \
    } else { \
      tag##label->h=tag##field->h; \
    } \
    y+=tag##label->h; \
  }
  KV(objid)
  KV(trim)
  KV(pan)
  #undef KV
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Activate.
 */

static int _ps_songevent_activate(struct ps_widget *widget) {
  return ps_songevent_cb_ok(0,widget);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_songevent={

  .name="songevent",
  .objlen=sizeof(struct ps_widget_songevent),

  .del=_ps_songevent_del,
  .init=_ps_songevent_init,

  .measure=_ps_songevent_measure,
  .pack=_ps_songevent_pack,

  .activate=_ps_songevent_activate,

};

/* Setup.
 */
 
int ps_widget_songevent_setup(struct ps_widget *widget,struct ps_sem *sem,int beatp,int eventp) {
  if (ps_songevent_obj_validate(widget)<0) return -1;
  if (!sem) return 0;
  if ((beatp<0)||(beatp>=sem->beatc)) return 0;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return 0;
  struct ps_sem_event *event=beat->eventv+eventp;

  if (ps_sem_ref(sem)<0) return -1;
  ps_sem_del(WIDGET->sem);
  WIDGET->sem=sem;
  WIDGET->beatp=beatp;
  WIDGET->eventp=eventp;

  struct ps_widget *infolabel=ps_songevent_get_infolabel(widget);
  char info[32];
  if (event->op==AKAU_SONG_OP_DRUM) {
    snprintf(info,sizeof(info),"Drum at %d:",beatp);
  } else if (event->op==AKAU_SONG_OP_NOTE) {
    snprintf(info,sizeof(info),"Note at %d:",beatp);
  } else {
    snprintf(info,sizeof(info),"Adjustment at %d:",beatp);
  }
  if (ps_widget_label_set_text(infolabel,info,-1)<0) return -1;

  struct ps_widget *objidlabel=ps_songevent_get_objidlabel(widget);
  if (event->op==AKAU_SONG_OP_DRUM) {
    if (ps_widget_label_set_text(objidlabel,"Drum ID",-1)<0) return -1;
  } else {
    if (ps_widget_label_set_text(objidlabel,"Instrument",-1)<0) return -1;
  }

  struct ps_widget *objidfield=ps_songevent_get_objidfield(widget);
  snprintf(info,sizeof(info),"%d",event->objid);
  if (ps_widget_field_set_text(objidfield,info,-1)<0) return -1;

  struct ps_widget *trimfield=ps_songevent_get_trimfield(widget);
  snprintf(info,sizeof(info),"%d",event->trim);
  if (ps_widget_field_set_text(trimfield,info,-1)<0) return -1;

  struct ps_widget *panfield=ps_songevent_get_panfield(widget);
  snprintf(info,sizeof(info),"%d",event->pan);
  if (ps_widget_field_set_text(panfield,info,-1)<0) return -1;
  
  return 0;
}

/* Read values from input fields.
 */

static int ps_songevent_get_values(int *objid,int *trim,int *pan,const struct ps_widget *widget) {
  struct ps_widget *field;
  const char *text;
  int textc,n;

  field=ps_songevent_get_objidfield(widget);
  if ((textc=ps_widget_field_get_text(&text,field))<0) return -1;
  if (ps_int_eval(&n,text,textc)<0) return -1;
  if ((n<0)||(n>255)) return -1;
  *objid=n;

  field=ps_songevent_get_trimfield(widget);
  if ((textc=ps_widget_field_get_text(&text,field))<0) return -1;
  if (ps_int_eval(&n,text,textc)<0) return -1;
  if ((n<0)||(n>255)) return -1;
  *trim=n;

  field=ps_songevent_get_panfield(widget);
  if ((textc=ps_widget_field_get_text(&text,field))<0) return -1;
  if (ps_int_eval(&n,text,textc)<0) return -1;
  if ((n<-128)||(n>127)) return -1;
  *pan=n;

  return 0;
}

/* Button callbacks.
 */
 
static int ps_songevent_cb_ok(struct ps_widget *button,struct ps_widget *widget) {

  int objid,trim,pan;
  if (ps_songevent_get_values(&objid,&trim,&pan,widget)<0) return 0; // Try again.

  if (ps_sem_modify_note(WIDGET->sem,WIDGET->beatp,WIDGET->eventp,objid,trim,pan)<0) return -1;
  
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_songevent_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}
