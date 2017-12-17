#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* Object definition.
 */

struct ps_widget_harmonics {
  struct ps_widget hdr;
  uint32_t barcolor;
  int mousex,mousey;
  int dragging;
};

#define WIDGET ((struct ps_widget_harmonics*)widget)

/* Delete.
 */

static void _ps_harmonics_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_harmonics_init(struct ps_widget *widget) {
  widget->accept_mouse_focus=1;
  widget->bgrgba=0x004000ff;
  WIDGET->barcolor=0xffff00ff;
  return 0;
}

/* Draw.
 */

static int _ps_harmonics_draw(struct ps_widget *widget,int parentx,int parenty) {

  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  if (widget->w<PS_WIDGET_WAVE_COEF_COUNT) return 0;
  if (widget->h<1) return 0;

  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;

  if (model->preset!=PS_WIDGET_WAVE_PRESET_HARMONICS) {
    return 0;
  }

  parentx+=widget->x;
  parenty+=widget->y;

  int right=widget->w;
  int i=PS_WIDGET_WAVE_COEF_COUNT; while (i-->0) {
    int left=(i*widget->w)/PS_WIDGET_WAVE_COEF_COUNT;
    int top=widget->h-(model->coefv[i]*widget->h);
    if (top<0) top=0;
    if (top<widget->h) {
      if (ps_video_draw_rect(parentx+left,parenty+top,right-left,widget->h-top,WIDGET->barcolor)<0) return -1;
    }
    right=left;
  }
  
  return 0;
}

/* Update model based on a click or drag at the given point.
 */

static int ps_harmonics_adjust_to_point(struct ps_widget *widget,int x,int y) {

  if ((widget->w<1)||(widget->h<1)) return 0;
  if (x<0) return 0;
  int coefp=(x*PS_WIDGET_WAVE_COEF_COUNT)/widget->w;
  if ((coefp<0)||(coefp>=PS_WIDGET_WAVE_COEF_COUNT)) return 0;
  double coef=(widget->h-y)/(double)(widget->h);
  if (coef<0.0) coef=0.0; else if (coef>1.0) coef=1.0;

  struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;

  if (coef==model->coefv[coefp]) return 0;
  model->coefv[coefp]=coef;
  if (ps_widget_wave_dirty(widget)<0) return -1;

  return 0;
}

/* Primitive input events.
 */

static int _ps_harmonics_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  if (WIDGET->dragging) {
    if (ps_harmonics_adjust_to_point(widget,x,y)<0) return -1;
  }
  return 0;
}

static int _ps_harmonics_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    if (value) {
      WIDGET->dragging=1;
      if (ps_harmonics_adjust_to_point(widget,WIDGET->mousex,WIDGET->mousey)<0) return -1;
    } else {
      WIDGET->dragging=0;
    }
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_harmonics={

  .name="harmonics",
  .objlen=sizeof(struct ps_widget_harmonics),

  .del=_ps_harmonics_del,
  .init=_ps_harmonics_init,

  .draw=_ps_harmonics_draw,

  .mousemotion=_ps_harmonics_mousemotion,
  .mousebutton=_ps_harmonics_mousebutton,

};

/* Update from parent's model.
 */

int ps_widget_harmonics_model_changed(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_harmonics)) return -1;
  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;
  // We reread the parent's model on every draw or input event. No action necessary here.
  return 0;
}
