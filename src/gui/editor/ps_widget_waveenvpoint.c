/* ps_widget_waveenvpoint.c
 * If I'd really thought it through, this and sfxpoint would be the same type.
 * But whatever.
 * This is a single point in the wave envelope editor.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

/* Object definition.
 */

struct ps_widget_waveenvpoint {
  struct ps_widget hdr;
  int dragging;
  int highlight;
  int usage;
};

#define WIDGET ((struct ps_widget_waveenvpoint*)widget)

/* Delete.
 */

static void _ps_waveenvpoint_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_waveenvpoint_init(struct ps_widget *widget) {

  widget->accept_mouse_focus=1;
  widget->draggable=1;
  
  return 0;
}

/* Draw.
 */

static int _ps_waveenvpoint_draw(struct ps_widget *widget,int parentx,int parenty) {
  struct akgl_vtx_mintile vtx={
    .x=parentx+widget->x+(widget->w>>1),
    .y=parenty+widget->y+(widget->h>>1),
    .tileid=WIDGET->dragging?0x12:WIDGET->highlight?0x11:0x10,
  };
  if (ps_video_draw_mintile(&vtx,1,0x01)<0) return -1;
  return 0;
}

/* Commit new position to model.
 */

static int ps_waveenvpoint_commit_new_position(struct ps_widget *widget) {
  if (!widget->parent||(widget->parent->w<1)||(widget->parent->h<1)) return 0;

  /* Dragging is managed by the root, and it allows to exceed the parent bounds.
   * Clamp to parent here.
   */
  int x=widget->x+(widget->w>>1);
  int y=widget->y+(widget->h>>1);
  if (x<0) x=0; else if (x>=widget->parent->w) x=widget->parent->w-1;
  if (y<0) y=0; else if (y>=widget->parent->h) y=widget->parent->h-1;
  widget->x=x-(widget->w>>1);
  widget->y=y-(widget->h>>1);

  /* Get model and time scale. Abort quietly if invalid. */
  int timescale=ps_widget_waveenv_get_timescale(widget);
  if (timescale<1) return 0;
  struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return 0;

  /* Transform (x,y) into (time,value). 
   * Value is always in 0..255.
   */
  int time=(x*timescale)/widget->parent->w;
  if (time<0) time=0;
  else if (time>timescale) time=timescale;
  int value=((widget->parent->h-y-1)*255)/widget->parent->h;
  if (value<0) value=0;
  else if (value>255) value=255;

  /* Apply to model based on usage.
   * The model contains relative times but for editing purposes they generally remain absolute.
   * ...except when the attack point ends up after the drawback point, then we change both.
   */
  switch (WIDGET->usage) {
    case PS_WIDGET_WAVEENVPOINT_USAGE_ATTACK: {
        model->drawback_time-=time-model->attack_time;
        if (model->drawback_time<0) model->drawback_time=0;
        model->attack_time=time;
        model->attack_trim=value;
      } break;
    case PS_WIDGET_WAVEENVPOINT_USAGE_DRAWBACK: {
        if (time<model->attack_time) model->attack_time=time;
        model->drawback_time=time-model->attack_time;
        model->drawback_trim=value;
      } break;
    default: return 0;
  }

  if (ps_widget_wave_dirty(widget)<0) return -1;
  
  return 0;
}

/* Input.
 */

static int _ps_waveenvpoint_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    if (value) {
      WIDGET->dragging=1;
    } else {
      WIDGET->dragging=0;
      if (ps_waveenvpoint_commit_new_position(widget)<0) return -1;
    }
  }
  return 0;
}

static int _ps_waveenvpoint_mouseenter(struct ps_widget *widget) {
  WIDGET->highlight=1;
  return 0;
}

static int _ps_waveenvpoint_mouseexit(struct ps_widget *widget) {
  WIDGET->highlight=0;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_waveenvpoint={

  .name="waveenvpoint",
  .objlen=sizeof(struct ps_widget_waveenvpoint),

  .del=_ps_waveenvpoint_del,
  .init=_ps_waveenvpoint_init,

  .draw=_ps_waveenvpoint_draw,

  .mousebutton=_ps_waveenvpoint_mousebutton,
  .mouseenter=_ps_waveenvpoint_mouseenter,
  .mouseexit=_ps_waveenvpoint_mouseexit,

};

/* Accessors.
 */
 
int ps_widget_waveenvpoint_set_usage(struct ps_widget *widget,int usage) {
  if (!widget||(widget->type!=&ps_widget_type_waveenvpoint)) return -1;
  WIDGET->usage=usage;
  return 0;
}

int ps_widget_waveenvpoint_get_usage(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_waveenvpoint)) return -1;
  return WIDGET->usage;
}
