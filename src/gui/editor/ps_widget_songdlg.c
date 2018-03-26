/* ps_widget_songdlg.c
 * Modal dialogue triggered by editsong to delete, duplicate, or modify a range of commands.
 *
 * In 'modify' mode, we present instrument, pitch, trim, and pan.
 * This can be either a precise number, which applies to all visible commands, or an adjustment.
 * Adjustments are phrased like "n+0" or "n-0".
 * Initially, if all values are the same, we present them precisely. Otherwise relatively.
 * Instrument/drum ID is not adjustable; it appears as "---" for multiple.
 *
 * Children:
 *   [0] okbutton
 *   [1] cancelbutton
 *   [2] message (textblock)
 *   [3] startlabel
 *   [4] startfield
 *   [5] endlabel
 *   [6] endfield
 * More children in 'modify' mode only:
 *   [7] instrumentlabel
 *   [8] instrumentfield
 *   [9] pitchlabel
 *   [10] pitchfield
 *   [11] trimlabel
 *   [12] trimfield
 *   [13] panlabel
 *   [14] panfield
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"
#include "resedit/ps_sem.h"
#include "util/ps_text.h"

#define PS_SONGDLG_MARGIN      5
#define PS_SONGDLG_SPACING     5
#define PS_SONGDLG_ROW_HEIGHT 16

static int ps_songdlg_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_songdlg_cb_cancel(struct ps_widget *button,struct ps_widget *widget);
static int ps_songdlg_cb_timeschanged(struct ps_widget *field,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_songdlg {
  struct ps_widget hdr;
  int mode;
  struct ps_callback cb;
  struct akau_song *song;
  struct ps_sem *sem;
  int startp,endp;
  uint8_t drum_visibility[32];
  uint8_t instrument_visibility[32];
};

#define WIDGET ((struct ps_widget_songdlg*)widget)

/* Delete.
 */

static void _ps_songdlg_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
  akau_song_del(WIDGET->song);
  ps_sem_del(WIDGET->sem);
}

/* Initialize.
 */

static int _ps_songdlg_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0xc0c0c0ff;

  if (!(child=ps_widget_button_spawn(widget,0,"OK",2,ps_callback(ps_songdlg_cb_ok,0,widget)))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Cancel",6,ps_callback(ps_songdlg_cb_cancel,0,widget)))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_textblock))) return -1;
  if (!(child=ps_widget_label_spawn(widget,"Start",5))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  if (ps_widget_field_set_cb_blur(child,ps_callback(ps_songdlg_cb_timeschanged,0,widget))<0) return -1;
  if (!(child=ps_widget_label_spawn(widget,"End (excl)",10))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_field))) return -1;
  if (ps_widget_field_set_cb_blur(child,ps_callback(ps_songdlg_cb_timeschanged,0,widget))<0) return -1;
  
  return 0;
}

/* Structural helpers.
 */

static int ps_songdlg_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_songdlg) return -1;
  switch (WIDGET->mode) {
    case PS_WIDGET_SONGDLG_MODE_MOD: if (widget->childc!=15) return -1; break;
    default: if (widget->childc!=7) return -1; break;
  }
  return 0;
}

static struct ps_widget *ps_songdlg_get_okbutton(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_songdlg_get_cancelbutton(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_songdlg_get_message(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_songdlg_get_startlabel(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_songdlg_get_startfield(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_songdlg_get_endlabel(const struct ps_widget *widget) { return widget->childv[5]; }
static struct ps_widget *ps_songdlg_get_endfield(const struct ps_widget *widget) { return widget->childv[6]; }

static struct ps_widget *ps_songdlg_get_instrumentlabel(const struct ps_widget *widget) { return widget->childv[7]; }
static struct ps_widget *ps_songdlg_get_instrumentfield(const struct ps_widget *widget) { return widget->childv[8]; }
static struct ps_widget *ps_songdlg_get_pitchlabel(const struct ps_widget *widget) { return widget->childv[9]; }
static struct ps_widget *ps_songdlg_get_pitchfield(const struct ps_widget *widget) { return widget->childv[10]; }
static struct ps_widget *ps_songdlg_get_trimlabel(const struct ps_widget *widget) { return widget->childv[11]; }
static struct ps_widget *ps_songdlg_get_trimfield(const struct ps_widget *widget) { return widget->childv[12]; }
static struct ps_widget *ps_songdlg_get_panlabel(const struct ps_widget *widget) { return widget->childv[13]; }
static struct ps_widget *ps_songdlg_get_panfield(const struct ps_widget *widget) { return widget->childv[14]; }

/* Measure.
 */

static int _ps_songdlg_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {

  *w=PS_SCREENW>>1;

  int chw,chh;
  if (ps_widget_measure(&chw,&chh,ps_songdlg_get_message(widget),*w,maxh)<0) return -1;
  *h=chh;
  
  switch (WIDGET->mode) {
  
    case PS_WIDGET_SONGDLG_MODE_MOD: {
        (*h)+=PS_SONGDLG_ROW_HEIGHT*7;
      } break;
      
    default: {
        (*h)+=PS_SONGDLG_ROW_HEIGHT*3;
      }
      
  }
  (*w)+=PS_SONGDLG_MARGIN<<1;
  (*h)+=PS_SONGDLG_MARGIN<<1;
  (*h)+=PS_SONGDLG_SPACING<<1; // One after the message, one before the buttons
  return 0;
}

/* Pack.
 */

static int _ps_songdlg_pack(struct ps_widget *widget) {
  if (ps_songdlg_obj_validate(widget)<0) return -1;
  struct ps_widget *okbutton=ps_songdlg_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_songdlg_get_cancelbutton(widget);
  struct ps_widget *message=ps_songdlg_get_message(widget);
  struct ps_widget *startlabel=ps_songdlg_get_startlabel(widget);
  struct ps_widget *startfield=ps_songdlg_get_startfield(widget);
  struct ps_widget *endlabel=ps_songdlg_get_endlabel(widget);
  struct ps_widget *endfield=ps_songdlg_get_endfield(widget);

  int y=PS_SONGDLG_MARGIN;
  int innerw=widget->w-(PS_SONGDLG_MARGIN<<1);
  int labelright=widget->w>>1;
  int chw,chh;

  /* Message at the top, full width, requested height. */
  if (ps_widget_measure(&chw,&chh,message,innerw,widget->h)<0) return -1;
  message->x=PS_SONGDLG_MARGIN;
  message->y=y;
  message->w=innerw;
  message->h=chh;
  y+=chh;
  y+=PS_SONGDLG_SPACING;

  #define FIELD(tag) { \
    if (ps_widget_measure(&chw,&chh,tag##label,innerw,widget->h)<0) return -1; \
    tag##label->x=labelright-chw; \
    tag##label->y=y; \
    tag##label->w=chw; \
    tag##label->h=PS_SONGDLG_ROW_HEIGHT; \
    tag##field->x=labelright+5; \
    tag##field->y=tag##label->y; \
    tag##field->w=widget->w-PS_SONGDLG_MARGIN-tag##field->x; \
    tag##field->h=PS_SONGDLG_ROW_HEIGHT; \
    y+=PS_SONGDLG_ROW_HEIGHT; \
  }

  FIELD(start)
  FIELD(end)

  if (WIDGET->mode==PS_WIDGET_SONGDLG_MODE_MOD) {
    struct ps_widget *instrumentlabel=ps_songdlg_get_instrumentlabel(widget);
    struct ps_widget *instrumentfield=ps_songdlg_get_instrumentfield(widget);
    struct ps_widget *pitchlabel=ps_songdlg_get_pitchlabel(widget);
    struct ps_widget *pitchfield=ps_songdlg_get_pitchfield(widget);
    struct ps_widget *trimlabel=ps_songdlg_get_trimlabel(widget);
    struct ps_widget *trimfield=ps_songdlg_get_trimfield(widget);
    struct ps_widget *panlabel=ps_songdlg_get_panlabel(widget);
    struct ps_widget *panfield=ps_songdlg_get_panfield(widget);
    FIELD(instrument)
    FIELD(pitch)
    FIELD(trim)
    FIELD(pan)
  }

  #undef FIELD

  if (ps_widget_measure(&chw,&chh,okbutton,innerw,widget->h)<0) return -1;
  okbutton->x=widget->w-PS_SONGDLG_MARGIN-chw;
  okbutton->y=widget->h-PS_SONGDLG_MARGIN-PS_SONGDLG_ROW_HEIGHT;
  okbutton->w=chw;
  okbutton->h=PS_SONGDLG_ROW_HEIGHT;
  if (ps_widget_measure(&chw,&chh,cancelbutton,innerw,widget->h)<0) return -1;
  cancelbutton->x=okbutton->x-chw;
  cancelbutton->y=okbutton->y;
  cancelbutton->w=chw;
  cancelbutton->h=okbutton->h;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Digested input events.
 */

static int _ps_songdlg_activate(struct ps_widget *widget) {
  return ps_songdlg_cb_ok(0,widget);
}

static int _ps_songdlg_cancel(struct ps_widget *widget) {
  return ps_songdlg_cb_cancel(0,widget);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_songdlg={

  .name="songdlg",
  .objlen=sizeof(struct ps_widget_songdlg),

  .del=_ps_songdlg_del,
  .init=_ps_songdlg_init,

  .measure=_ps_songdlg_measure,
  .pack=_ps_songdlg_pack,

  .activate=_ps_songdlg_activate,
  .cancel=_ps_songdlg_cancel,

};

/* Test validity of model and range.
 */

static int ps_songdlg_range_valid(const struct ps_widget *widget) {
  if (!WIDGET->song) return 0;
  if (!WIDGET->sem) return 0;
  if (WIDGET->startp<0) return 0;
  if (WIDGET->startp>WIDGET->endp) return 0;
  if (WIDGET->endp>WIDGET->sem->beatc) return 0;
  return 1;
}

/* Commit duplication. Range is already validated.
 * This does not copy adjustments (TODO)
 */

static int ps_songdlg_execute_DUP(struct ps_widget *widget) {

  /* Add blank beats to the sem. */
  int dstp=WIDGET->sem->beatc;
  int srcc=WIDGET->endp-WIDGET->startp;
  if (ps_sem_insert_beats(WIDGET->sem,dstp,srcc)<0) return -1;

  /* Copy visible events from our focus range into the new range. */
  const struct ps_sem_beat *srcbeat=WIDGET->sem->beatv+WIDGET->startp;
  int i=0; for (;i<srcc;i++,srcbeat++) {
    const struct ps_sem_event *srcevent=srcbeat->eventv;
    int srceventp=0; for (;srceventp<srcbeat->eventc;srceventp++,srcevent++) {
      switch (srcevent->op) {
      
        case AKAU_SONG_OP_DRUM: {
            if (!PS_EDITSONG_TEST_VOICE(WIDGET->drum_visibility,srcevent->objid)) continue;
            if (ps_sem_add_drum(WIDGET->sem,dstp+i,srcevent->objid,srcevent->trim,srcevent->pan)<0) return -1;
          } break;
          
        case AKAU_SONG_OP_NOTE: {
            if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,srcevent->objid)) continue;
            int noteid=ps_sem_add_note(WIDGET->sem,dstp+i,srcevent->objid,srcevent->pitch,srcevent->trim,srcevent->pan);
            if (noteid<0) return -1;
            int srcbeatp=WIDGET->startp+i;
            int srcpartnerbeatp,srcpartnereventp;
            if (ps_sem_find_partner(&srcpartnerbeatp,&srcpartnereventp,WIDGET->sem,WIDGET->startp+i,srceventp)<0) return -1;
            int notelen=srcpartnerbeatp-srcbeatp+1;
            if (ps_sem_set_note_length(WIDGET->sem,noteid,notelen)<0) return -1;
          } break;
      }
    }
  }

  return 0;
}

/* Commit deletion. Range is already validated.
 */

static int ps_songdlg_execute_DEL(struct ps_widget *widget) {
  if (ps_sem_remove_beats(WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp)<0) return -1;
  return 0;
}

/* Commit modification. Range is already validated.
 */

static int ps_songdlg_filter_drumid(uint8_t drumid,void *userdata) {
  struct ps_widget *widget=userdata;
  return PS_EDITSONG_TEST_VOICE(WIDGET->drum_visibility,drumid);
}

static int ps_songdlg_filter_instrid(uint8_t instrid,void *userdata) {
  struct ps_widget *widget=userdata;
  return PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,instrid);
}

static int ps_songdlg_read_parameter(int *v,int *adjust,const struct ps_widget *field) {
  const char *src=0;
  int srcc=ps_widget_field_get_text(&src,field);
  if (!src||(srcc<=0)) return -1;
  if (src[0]=='n') {
    *adjust=1;
    if (ps_int_eval(v,src+1,srcc-1)<0) return -1;
    if (!*v) return 0; // No change.
  } else {
    *adjust=0;
    if (ps_int_eval(v,src,srcc)<0) return -1;
  }
  return 1;
}

static int ps_songdlg_execute_MOD(struct ps_widget *widget) {
  int nv,adjust;

  ps_log(EDIT,TRACE,"execute song modification...");

  if (ps_songdlg_read_parameter(&nv,&adjust,ps_songdlg_get_instrumentfield(widget))>0) {
    ps_log(EDIT,TRACE,"instrument %d %s",nv,adjust?"relative":"absolute");
    if (adjust) {
      ps_log(EDIT,WARN,"Ignoring relative adjustment to voice ID, that was probably a mistake.");
    } else if ((nv<0)||(nv>255)) {
      ps_log(EDIT,ERROR,"Invalid object ID %d",nv);
      return -1;
    } else {
      if (ps_sem_set_objid_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,nv,ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    }
  }

  if (ps_songdlg_read_parameter(&nv,&adjust,ps_songdlg_get_pitchfield(widget))>0) {
    ps_log(EDIT,TRACE,"pitch %d %s",nv,adjust?"relative":"absolute");
    if (adjust) {
      if (ps_sem_adjust_pitch_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    } else if ((nv<0)||(nv>255)) {
      ps_log(EDIT,ERROR,"Invalid pitch %d",nv);
      return -1;
    } else {
      if (ps_sem_set_pitch_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    }
  }

  if (ps_songdlg_read_parameter(&nv,&adjust,ps_songdlg_get_trimfield(widget))>0) {
    ps_log(EDIT,TRACE,"trim %d %s",nv,adjust?"relative":"absolute");
    if (adjust) {
      if (ps_sem_adjust_trim_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    } else if ((nv<0)||(nv>255)) {
      ps_log(EDIT,ERROR,"Invalid trim %d",nv);
      return -1;
    } else {
      if (ps_sem_set_trim_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    }
  }

  if (ps_songdlg_read_parameter(&nv,&adjust,ps_songdlg_get_panfield(widget))>0) {
    ps_log(EDIT,TRACE,"pan %d %s",nv,adjust?"relative":"absolute");
    if (adjust) {
      if (ps_sem_adjust_pan_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    } else if ((nv<-128)||(nv>127)) {
      ps_log(EDIT,ERROR,"Invalid pan %d",nv);
      return -1;
    } else {
      if (ps_sem_set_pan_in_range(
        WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
        nv,ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,widget
      )<0) return -1;
    }
  }
  
  return 0;
}

/* Read parameters from UI and attempt to change model objects.
 */

static int ps_songdlg_execute(struct ps_widget *widget) {
  if (ps_widget_field_get_integer(&WIDGET->startp,ps_songdlg_get_startfield(widget))<0) return -1;
  if (ps_widget_field_get_integer(&WIDGET->endp,ps_songdlg_get_endfield(widget))<0) return -1;
  if (!ps_songdlg_range_valid(widget)) {
    ps_log(EDIT,ERROR,"Range %d..%d invalid for SEM length %d",WIDGET->startp,WIDGET->endp,WIDGET->sem?WIDGET->sem->beatc:0);
    return -1;
  }
  switch (WIDGET->mode) {
    case PS_WIDGET_SONGDLG_MODE_DUP: return ps_songdlg_execute_DUP(widget);
    case PS_WIDGET_SONGDLG_MODE_DEL: return ps_songdlg_execute_DEL(widget);
    case PS_WIDGET_SONGDLG_MODE_MOD: return ps_songdlg_execute_MOD(widget);
  }
  return -1;
}

/* Represent modification parameters.
 */
 
static int ps_songdlg_repr_instrument_mod(char *dst,int dsta,const struct ps_widget *widget) {
  
  uint8_t drumidlo,drumidhi,instridlo,instridhi;
  if (ps_sem_get_objid_in_range(
    &drumidlo,&drumidhi,&instridlo,&instridhi,
    WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
    ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,(void*)widget
  )<0) return -1;

  if (drumidlo>drumidhi) { // No drums...
    if (instridlo>instridhi) { // Nothing
      return ps_strcpy(dst,dsta,"---",3);
    } else if (instridlo==instridhi) { // Single instrument
      return ps_decsint_repr(dst,dsta,instridlo);
    } else { // Multiple instruments
      return ps_strcpy(dst,dsta,"---",3);
    }
  } else if (instridlo>instridhi) { // No instruments...
    if (drumidlo==drumidhi) { // Single drum
      return ps_decsint_repr(dst,dsta,drumidlo);
    } else { // Multiple drums
      return ps_strcpy(dst,dsta,"---",3);
    }
  } else { // Drums and instruments
    return ps_strcpy(dst,dsta,"---",3);
  }
  return -1; // unreachable
}

static int ps_songdlg_repr_mutation_range(char *dst,int dsta,int lo,int hi) {
  if (lo>hi) return ps_strcpy(dst,dsta,"---",3);
  if (lo==hi) return ps_decsint_repr(dst,dsta,lo);
  return ps_strcpy(dst,dsta,"n+0",3);
}
 
static int ps_songdlg_repr_pitch_mod(char *dst,int dsta,const struct ps_widget *widget) {
  uint8_t lo,hi;
  if (ps_sem_get_pitch_in_range(
    &lo,&hi,
    WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
    ps_songdlg_filter_instrid,(void*)widget
  )<0) return -1;
  return ps_songdlg_repr_mutation_range(dst,dsta,lo,hi);
}
 
static int ps_songdlg_repr_trim_mod(char *dst,int dsta,const struct ps_widget *widget) {
  uint8_t lo,hi;
  if (ps_sem_get_trim_in_range(
    &lo,&hi,
    WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
    ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,(void*)widget
  )<0) return -1;
  return ps_songdlg_repr_mutation_range(dst,dsta,lo,hi);
}
 
static int ps_songdlg_repr_pan_mod(char *dst,int dsta,const struct ps_widget *widget) {
  int8_t lo,hi;
  if (ps_sem_get_pan_in_range(
    &lo,&hi,
    WIDGET->sem,WIDGET->startp,WIDGET->endp-WIDGET->startp,
    ps_songdlg_filter_drumid,ps_songdlg_filter_instrid,(void*)widget
  )<0) return -1;
  return ps_songdlg_repr_mutation_range(dst,dsta,lo,hi);
}

/* Populate UI from model.
 */

static int ps_songdlg_populate_ui_from_model(struct ps_widget *widget) {

  struct ps_widget *startfield=ps_songdlg_get_startfield(widget);
  struct ps_widget *endfield=ps_songdlg_get_endfield(widget);
  startfield->bgrgba=0xffffffff;
  endfield->bgrgba=0xffffffff;
  if (ps_widget_field_set_integer(startfield,WIDGET->startp)<0) return -1;
  if (ps_widget_field_set_integer(endfield,WIDGET->endp)<0) return -1;

  switch (WIDGET->mode) {
  
    case PS_WIDGET_SONGDLG_MODE_DUP: {
        if (ps_widget_textblock_set_text(ps_songdlg_get_message(widget),"Append to the song by copying visible commands from this range:",-1)<0) return -1;
      } break;
      
    case PS_WIDGET_SONGDLG_MODE_DEL: {
        if (ps_widget_textblock_set_text(ps_songdlg_get_message(widget),"Delete this range:",-1)<0) return -1;
      } break;
      
    case PS_WIDGET_SONGDLG_MODE_MOD: {
        if (ps_widget_textblock_set_text(ps_songdlg_get_message(widget),"Modify visible commands in this range:",-1)<0) return -1;
        if (ps_songdlg_range_valid(widget)) {
          char buf[32];
          int bufc;
          #define FIELD(tag) { \
            bufc=ps_songdlg_repr_##tag##_mod(buf,sizeof(buf),widget); \
            if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0; \
            if (ps_widget_field_set_text(ps_songdlg_get_##tag##field(widget),buf,bufc)<0) return -1; \
          }
          FIELD(instrument)
          FIELD(pitch)
          FIELD(trim)
          FIELD(pan)
          #undef FIELD
        } else {
          if (ps_widget_field_set_text(ps_songdlg_get_instrumentfield(widget),"---",3)<0) return -1;
          if (ps_widget_field_set_text(ps_songdlg_get_pitchfield(widget),"---",3)<0) return -1;
          if (ps_widget_field_set_text(ps_songdlg_get_trimfield(widget),"---",3)<0) return -1;
          if (ps_widget_field_set_text(ps_songdlg_get_panfield(widget),"---",3)<0) return -1;
        }
      } break;
  }

  return 0;
}

/* Populate UI in modify mode when range is invalid.
 */

static int ps_songdlg_populate_ui_for_invalid_range(struct ps_widget *widget) {
  struct ps_widget *startfield=ps_songdlg_get_startfield(widget);
  struct ps_widget *endfield=ps_songdlg_get_endfield(widget);
  startfield->bgrgba=0xffc0c0ff;
  endfield->bgrgba=0xffc0c0ff;
  if (WIDGET->mode==PS_WIDGET_SONGDLG_MODE_MOD) {
    if (ps_widget_field_set_text(ps_songdlg_get_instrumentfield(widget),"---",3)<0) return -1;
    if (ps_widget_field_set_text(ps_songdlg_get_pitchfield(widget),"---",3)<0) return -1;
    if (ps_widget_field_set_text(ps_songdlg_get_trimfield(widget),"---",3)<0) return -1;
    if (ps_widget_field_set_text(ps_songdlg_get_panfield(widget),"---",3)<0) return -1;
  }
  return 0;
}

/* Setup.
 */
 
int ps_widget_songdlg_setup(
  struct ps_widget *widget,
  struct ps_widget *editsong,
  int mode,
  struct ps_callback cb
) {
  if (ps_songdlg_obj_validate(widget)<0) return -1;
  if (WIDGET->mode) return -1;
  struct akau_song *song=ps_widget_editsong_get_song(editsong);
  struct ps_sem *sem=ps_widget_editsong_get_sem(editsong);

  if (ps_widget_editsong_get_voice_visibility(WIDGET->drum_visibility,WIDGET->instrument_visibility,editsong)<0) return -1;

  /* Create additional fields for mod mode. */
  if (mode==PS_WIDGET_SONGDLG_MODE_MOD) {
    if (!ps_widget_label_spawn(widget,"Voice ID",-1)) return -1;
    if (!ps_widget_spawn(widget,&ps_widget_type_field)) return -1;
    if (!ps_widget_label_spawn(widget,"Pitch",-1)) return -1;
    if (!ps_widget_spawn(widget,&ps_widget_type_field)) return -1;
    if (!ps_widget_label_spawn(widget,"Trim",-1)) return -1;
    if (!ps_widget_spawn(widget,&ps_widget_type_field)) return -1;
    if (!ps_widget_label_spawn(widget,"Pan",-1)) return -1;
    if (!ps_widget_spawn(widget,&ps_widget_type_field)) return -1;
  }

  if (akau_song_ref(song)<0) return -1;
  WIDGET->song=song;
  if (ps_sem_ref(sem)<0) return -1;
  WIDGET->sem=sem;

  WIDGET->cb=cb;
  WIDGET->mode=mode;
  WIDGET->startp=0;
  WIDGET->endp=sem->beatc;

  if (ps_songdlg_populate_ui_from_model(widget)<0) return -1;

  return 0;
}

/* Button callbacks.
 */
 
static int ps_songdlg_cb_ok(struct ps_widget *button,struct ps_widget *widget) {

  if (ps_songdlg_execute(widget)<0) {
    if (!ps_widget_spawn_dialogue_message(ps_widget_get_root(widget),"Failed to modify song. Check parameters.",-1,0)) return -1;
    return 0;
  }

  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_songdlg_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Respond to change of start or end time.
 */
 
static int ps_songdlg_cb_timeschanged(struct ps_widget *field,struct ps_widget *widget) {
  if (WIDGET->mode==PS_WIDGET_SONGDLG_MODE_MOD) {
    int nstartp,nendp;
    if (
      (ps_widget_field_get_integer(&nstartp,ps_songdlg_get_startfield(widget))<0)||
      (ps_widget_field_get_integer(&nendp,ps_songdlg_get_endfield(widget))<0)||
      (nstartp<0)||(nstartp>nendp)||!WIDGET->sem||(nendp>WIDGET->sem->beatc)
    ) {
      if (ps_songdlg_populate_ui_for_invalid_range(widget)<0) return -1;
    } else {
      WIDGET->startp=nstartp;
      WIDGET->endp=nendp;
      if (ps_songdlg_populate_ui_from_model(widget)<0) return -1;
    }
  }
  return 0;
}
