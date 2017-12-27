/* ps_widget_waveenv.c
 * Visual editor for instrument envelope.
 * We draw a graph of attack and drawback, and text entry for the decay.
 * There are static labels showing the precise attack and drawback values.
 *
 * Children:
 *  [0] attackpoint
 *  [1] drawbackpoint
 *  [2] decaybutton
 *  [3] attacklabel
 *  [4] drawbacklabel
 *  [5] scalebutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "ps_editor.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_WAVEENV_POINT_SIZE 8
#define PS_WAVEENV_TIMESCALE_SANITY_LIMIT 10000

static int ps_waveenv_cb_decay(struct ps_widget *button,struct ps_widget *widget);
static int ps_waveenv_cb_timescale(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_waveenv {
  struct ps_widget hdr;
  int timescale;
  int reset_scale; // nonzero initially; next time model changes we adjust scale
};

#define WIDGET ((struct ps_widget_waveenv*)widget)

/* Delete.
 */

static void _ps_waveenv_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_waveenv_init(struct ps_widget *widget) {

  widget->bgrgba=0x404020ff;
  widget->fgrgba=0xffff00ff;
  WIDGET->timescale=1000; // Will reset when the model is acquired.
  WIDGET->reset_scale=1;

  struct ps_widget *child;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_waveenvpoint))) return -1;
    if (ps_widget_waveenvpoint_set_usage(child,PS_WIDGET_WAVEENVPOINT_USAGE_ATTACK)<0) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_waveenvpoint))) return -1;
    if (ps_widget_waveenvpoint_set_usage(child,PS_WIDGET_WAVEENVPOINT_USAGE_DRAWBACK)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1;
    if (ps_widget_button_set_callback(child,ps_callback(ps_waveenv_cb_decay,0,widget))<0) return -1;
    if (ps_widget_button_set_text(child,"Decay",5)<0) return -1;
    
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
    if (ps_widget_label_set_text(child,"[1]",3)<0) return -1;
    child->fgrgba=0x00ffffff;
    
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
    if (ps_widget_label_set_text(child,"[2]",3)<0) return -1;
    child->fgrgba=0x00ffffff;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1;
    if (ps_widget_button_set_callback(child,ps_callback(ps_waveenv_cb_timescale,0,widget))<0) return -1;
    if (ps_widget_button_set_text(child,"1000",4)<0) return -1;
  
  return 0;
}

/* Structural helpers.
 */

static int ps_waveenv_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_waveenv) return -1;
  if (widget->childc!=6) return -1;
  return 0;
}

static struct ps_widget *ps_waveenv_get_attackpoint(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_waveenv_get_drawbackpoint(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_waveenv_get_decaybutton(const struct ps_widget *widget) {
  return widget->childv[2];
}

static struct ps_widget *ps_waveenv_get_attacklabel(const struct ps_widget *widget) {
  return widget->childv[3];
}

static struct ps_widget *ps_waveenv_get_drawbacklabel(const struct ps_widget *widget) {
  return widget->childv[4];
}

static struct ps_widget *ps_waveenv_get_scalebutton(const struct ps_widget *widget) {
  return widget->childv[5];
}

/* Draw.
 */

static int ps_waveenv_draw_line(struct ps_widget *widget,int parentx,int parenty) {
  const struct ps_widget *attackpoint=ps_waveenv_get_attackpoint(widget);
  const struct ps_widget *drawbackpoint=ps_waveenv_get_drawbackpoint(widget);
  parentx+=widget->x;
  parenty+=widget->y;

  uint8_t r=widget->fgrgba>>24;
  uint8_t g=widget->fgrgba>>16;
  uint8_t b=widget->fgrgba>>8;
  uint8_t a=widget->fgrgba;
  struct akgl_vtx_raw vtxv[4]={
    {parentx,parenty+widget->h,r,g,b,a},
    {parentx+attackpoint->x+(attackpoint->w>>1),parenty+attackpoint->y+(attackpoint->h>>1),r,g,b,a},
    {parentx+drawbackpoint->x+(drawbackpoint->w>>1),parenty+drawbackpoint->y+(drawbackpoint->h>>1),r,g,b,a},
    {parentx+widget->w,parenty+drawbackpoint->y+(drawbackpoint->h>>1),r,g,b,a},
  };

  if (ps_video_draw_line_strip(vtxv,4)<0) return -1;
  
  return 0;
}

static int _ps_waveenv_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_waveenv_draw_line(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_waveenv_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Convert coordinates to and from model.
 */

static int ps_waveenv_horz_from_time(const struct ps_widget *widget,int time) {
  if (WIDGET->timescale<1) return 0;
  int x=(time*widget->w)/WIDGET->timescale;
  if (x<0) return 0;
  if (x>widget->w) return widget->w;
  return x;
}

static int ps_waveenv_time_from_horz(const struct ps_widget *widget,int x) {
  if (widget->w<1) return 0;
  int time=(x*WIDGET->timescale)/widget->w;
  if (time<0) return 0;
  if (time>WIDGET->timescale) return WIDGET->timescale;
  return time;
}

static int ps_waveenv_vert_from_trim(const struct ps_widget *widget,int trim) {
  int y=widget->h-1-(trim*widget->h)/255;
  if (y<0) return 0;
  if (y>widget->h) return widget->h;
  return y;
}

static int ps_waveenv_trim_from_vert(const struct ps_widget *widget,int y) {
  if (widget->h<1) return 0;
  int trim=((widget->h-y-1)*255)/widget->h;
  if (trim<0) return 0;
  if (trim>255) return 255;
  return trim;
}

/* Pack.
 */

static int _ps_waveenv_pack(struct ps_widget *widget) {
  if ((widget->w<1)||(widget->h<1)) return 0;

  if (ps_waveenv_obj_validate(widget)<0) return -1;
  struct ps_widget *attackpoint=ps_waveenv_get_attackpoint(widget);
  struct ps_widget *drawbackpoint=ps_waveenv_get_drawbackpoint(widget);
  struct ps_widget *decaybutton=ps_waveenv_get_decaybutton(widget);
  struct ps_widget *attacklabel=ps_waveenv_get_attacklabel(widget);
  struct ps_widget *drawbacklabel=ps_waveenv_get_drawbacklabel(widget);
  struct ps_widget *scalebutton=ps_waveenv_get_scalebutton(widget);

  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;

  /* Size of points is fixed -- easy. */
  attackpoint->w=PS_WAVEENV_POINT_SIZE;
  attackpoint->h=PS_WAVEENV_POINT_SIZE;
  drawbackpoint->w=PS_WAVEENV_POINT_SIZE;
  drawbackpoint->h=PS_WAVEENV_POINT_SIZE;

  /* Calculate position of points based on the model and my size. */
  attackpoint->x=ps_waveenv_horz_from_time(widget,model->attack_time)-(attackpoint->w>>1);
  attackpoint->y=ps_waveenv_vert_from_trim(widget,model->attack_trim)-(attackpoint->h>>1);
  drawbackpoint->x=ps_waveenv_horz_from_time(widget,model->attack_time+model->drawback_time)-(drawbackpoint->w>>1);
  drawbackpoint->y=ps_waveenv_vert_from_trim(widget,model->drawback_trim)-(drawbackpoint->h>>1);

  /* Put the labels and buttons along the right edge, down from the top, whatever size they ask for.
   */
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,attacklabel,widget->w,widget->h/4)<0) return -1;
  attacklabel->x=widget->w-chw;
  attacklabel->w=chw;
  attacklabel->y=0;
  attacklabel->h=chh;
  if (ps_widget_measure(&chw,&chh,drawbacklabel,widget->w,widget->h/4)<0) return -1;
  drawbacklabel->x=widget->w-chw;
  drawbacklabel->w=chw;
  drawbacklabel->y=attacklabel->y+attacklabel->h;
  drawbacklabel->h=chh;
  if (ps_widget_measure(&chw,&chh,decaybutton,widget->w,widget->h/4)<0) return -1;
  decaybutton->x=widget->w-chw;
  decaybutton->w=chw;
  decaybutton->y=drawbacklabel->y+drawbacklabel->h;
  decaybutton->h=chh;

  /* Scale button goes in the lower right. */
  if (ps_widget_measure(&chw,&chh,scalebutton,widget->w,widget->h/4)<0) return -1;
  scalebutton->x=widget->w-chw;
  scalebutton->y=widget->h-chh;
  scalebutton->w=chw;
  scalebutton->h=chh;

  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_waveenv_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_waveenv_mousemotion(struct ps_widget *widget,int x,int y) {
  return 0;
}

static int _ps_waveenv_mousebutton(struct ps_widget *widget,int btnid,int value) {
  return 0;
}

static int _ps_waveenv_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_waveenv_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_waveenv_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_waveenv_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_waveenv_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_waveenv_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_waveenv_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_waveenv_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_waveenv_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_waveenv_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_waveenv={

  .name="waveenv",
  .objlen=sizeof(struct ps_widget_waveenv),

  .del=_ps_waveenv_del,
  .init=_ps_waveenv_init,

  .draw=_ps_waveenv_draw,
  .measure=_ps_waveenv_measure,
  .pack=_ps_waveenv_pack,

  .update=_ps_waveenv_update,
  .mousemotion=_ps_waveenv_mousemotion,
  .mousebutton=_ps_waveenv_mousebutton,
  .mousewheel=_ps_waveenv_mousewheel,
  .key=_ps_waveenv_key,
  .userinput=_ps_waveenv_userinput,

  .mouseenter=_ps_waveenv_mouseenter,
  .mouseexit=_ps_waveenv_mouseexit,
  .activate=_ps_waveenv_activate,
  .cancel=_ps_waveenv_cancel,
  .adjust=_ps_waveenv_adjust,
  .focus=_ps_waveenv_focus,
  .unfocus=_ps_waveenv_unfocus,

};

/* Notify of parent model change.
 */
 
int ps_widget_waveenv_model_changed(struct ps_widget *widget) {
  if (ps_waveenv_obj_validate(widget)<0) return -1;
  struct ps_widget *decaybutton=ps_waveenv_get_decaybutton(widget);
  struct ps_widget *attacklabel=ps_waveenv_get_attacklabel(widget);
  struct ps_widget *drawbacklabel=ps_waveenv_get_drawbacklabel(widget);
  char buf[32];
  int bufc;

  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;

  bufc=snprintf(buf,sizeof(buf),"Decay: %d",model->decay_time);
  if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
  if (ps_widget_button_set_text(decaybutton,buf,bufc)<0) return -1;

  bufc=snprintf(buf,sizeof(buf),"Attack: %d @ %d",model->attack_trim,model->attack_time);
  if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
  if (ps_widget_label_set_text(attacklabel,buf,bufc)<0) return -1;

  bufc=snprintf(buf,sizeof(buf),"Drawback: %d @ %d",model->drawback_trim,model->drawback_time);
  if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
  if (ps_widget_label_set_text(drawbacklabel,buf,bufc)<0) return -1;

  /* First time we acquire the model, set scale such that the drawback point is at around 2/3. */
  if (WIDGET->reset_scale) {
    WIDGET->reset_scale=0;
    WIDGET->timescale=((model->attack_time+model->drawback_time)*3)/2;
    struct ps_widget *scalebutton=ps_waveenv_get_scalebutton(widget);
    char buf[32];
    int bufc=snprintf(buf,sizeof(buf),"%d",WIDGET->timescale);
    if (ps_widget_button_set_text(scalebutton,buf,bufc)<0) return -1;
  }
  
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Edit decay.
 */

static int ps_waveenv_cb_decay_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  int decay=0;
  if (ps_widget_dialogue_get_number(&decay,dialogue)<0) return 0;
  if ((decay<0)||(decay>0xffff)) return 0;

  if (decay!=model->decay_time) {
    model->decay_time=decay;

    struct ps_widget *decaybutton=ps_waveenv_get_decaybutton(widget);
    char buf[32];
    int bufc=snprintf(buf,sizeof(buf),"Decay: %d",model->decay_time);
    if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
    if (ps_widget_button_set_text(decaybutton,buf,bufc)<0) return -1;
    
    ps_log(EDIT,TRACE,"%s",__func__);
    if (ps_widget_wave_dirty(widget)<0) return -1;
    ps_log(EDIT,TRACE,"%s",__func__);
  }

  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}
 
static int ps_waveenv_cb_decay(struct ps_widget *button,struct ps_widget *widget) {

  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Decay (ms):",-1,
    model->decay_time,
    ps_waveenv_cb_decay_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  
  return 0;
}

/* Edit scale.
 */

static int ps_waveenv_cb_timescale_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int timescale=0;
  if (ps_widget_dialogue_get_number(&timescale,dialogue)<0) return 0;
  if ((timescale<1)||(timescale>PS_WAVEENV_TIMESCALE_SANITY_LIMIT)) return 0;

  if (timescale!=WIDGET->timescale) {
    WIDGET->timescale=timescale;
    struct ps_widget *scalebutton=ps_waveenv_get_scalebutton(widget);
    char buf[32];
    int bufc=snprintf(buf,sizeof(buf),"%d",WIDGET->timescale);
    if (ps_widget_button_set_text(scalebutton,buf,bufc)<0) return -1;
    if (ps_widget_pack(widget)<0) return -1;
  }

  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}
 
static int ps_waveenv_cb_timescale(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Time scale (ms):",-1,
    WIDGET->timescale,
    ps_waveenv_cb_timescale_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;

  return 0;
}

/* Accessors.
 */
 
int ps_widget_waveenv_get_timescale(const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_waveenv)) widget=widget->parent;
  if (!widget) return -1;
  return WIDGET->timescale;
}
