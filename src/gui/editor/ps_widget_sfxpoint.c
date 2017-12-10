#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"
#include "resedit/ps_iwg.h"

static int ps_sfxpoint_remove_self(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_sfxpoint {
  struct ps_widget hdr;
  int cmdp;
  int time;
  double value;
  int dragging;
  int highlight;
};

#define WIDGET ((struct ps_widget_sfxpoint*)widget)

/* Delete.
 */

static void _ps_sfxpoint_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_sfxpoint_init(struct ps_widget *widget) {

  widget->accept_mouse_focus=1;
  widget->draggable=1;

  WIDGET->cmdp=-1;

  return 0;
}

/* Get iwg from ancestor.
 */

static struct ps_iwg *ps_sfxpoint_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  widget=widget->parent; // sfxgraph
  if (!widget) return 0;
  widget=widget->parent; // sfxchan
  if (!widget) return 0;
  widget=widget->parent; // scrolllist
  if (!widget) return 0;
  widget=widget->parent; // editsoundeffect
  return ps_widget_editsoundeffect_get_iwg(widget);
}

/* General properties.
 */

static int _ps_sfxpoint_get_property(int *v,const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return -1;
}

static int _ps_sfxpoint_set_property(struct ps_widget *widget,int k,int v) {
  switch (k) {
  }
  return -1;
}

static int _ps_sfxpoint_get_property_type(const struct ps_widget *widget,int k) {
  switch (k) {
  }
  return PS_WIDGET_PROPERTY_TYPE_UNDEFINED;
}

/* Draw.
 */

static int _ps_sfxpoint_draw(struct ps_widget *widget,int parentx,int parenty) {
  struct akgl_vtx_mintile vtx={
    .x=parentx+widget->x+(widget->w>>1),
    .y=parenty+widget->y+(widget->h>>1),
    .tileid=WIDGET->dragging?0x12:WIDGET->highlight?0x11:0x10,
  };
  if (ps_video_draw_mintile(&vtx,1,0x01)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_sfxpoint_update(struct ps_widget *widget) {
  return 0;
}

/* Done dragging, update time and value from position.
 */

static int ps_sfxpoint_commit_new_position(struct ps_widget *widget) {
  if (!widget->parent) return 0;

  int midx=widget->x+(widget->w>>1);
  if (midx<0) midx=0; else if (midx>widget->parent->w) midx=widget->parent->w;
  int midy=widget->y+(widget->h>>1);
  if (midy<0) midy=0; else if (midy>widget->parent->h) midy=widget->parent->h;
  widget->x=midx-(widget->w>>1);
  widget->y=midy-(widget->h>>1);

  struct ps_iwg *iwg=ps_sfxpoint_get_iwg(widget);
  if (!iwg) return 0;
  if (iwg->cmdc<1) return 0;
  if ((WIDGET->cmdp<0)||(WIDGET->cmdp>=iwg->cmdc)) return 0;
  int timez=iwg->cmdv[iwg->cmdc-1].time;
  double valuez=ps_widget_sfxgraph_get_value_limit(widget->parent);
  if (valuez<=0.0) return 0;

  int ntime=(midx*timez)/widget->parent->w;
  double nvalue=((widget->parent->h-midy)*valuez)/widget->parent->h;

  if (ntime<0) ntime=0;
  if (nvalue<0.0) nvalue=0.0;

  iwg->cmdv[WIDGET->cmdp].time=WIDGET->time=ntime;
  iwg->cmdv[WIDGET->cmdp].v=WIDGET->value=nvalue;
  iwg->dirty=1;
  
  return 0;
}

/* Mouse events.
 */

static int _ps_sfxpoint_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    if (value) {
      WIDGET->dragging=1;
    } else {
      WIDGET->dragging=0;
      if (ps_sfxpoint_commit_new_position(widget)<0) return -1;
    }
  } else if (btnid==3) {
    if (value) {
      if (ps_sfxpoint_remove_self(widget)<0) return -1;
    }
  }
  return 0;
}

static int _ps_sfxpoint_mouseenter(struct ps_widget *widget) {
  WIDGET->highlight=1;
  return 0;
}

static int _ps_sfxpoint_mouseexit(struct ps_widget *widget) {
  WIDGET->highlight=0;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_sfxpoint={

  .name="sfxpoint",
  .objlen=sizeof(struct ps_widget_sfxpoint),

  .del=_ps_sfxpoint_del,
  .init=_ps_sfxpoint_init,

  .get_property=_ps_sfxpoint_get_property,
  .set_property=_ps_sfxpoint_set_property,
  .get_property_type=_ps_sfxpoint_get_property_type,

  .draw=_ps_sfxpoint_draw,

  .mousebutton=_ps_sfxpoint_mousebutton,
  .mouseenter=_ps_sfxpoint_mouseenter,
  .mouseexit=_ps_sfxpoint_mouseexit,

};

/* Accessors.
 */
 
int ps_widget_sfxpoint_set_time(struct ps_widget *widget,int time) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return -1;
  WIDGET->time=time;
  return 0;
}

int ps_widget_sfxpoint_set_value(struct ps_widget *widget,double value) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return -1;
  WIDGET->value=value;
  return 0;
}

int ps_widget_sfxpoint_set_cmdp(struct ps_widget *widget,int cmdp) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return -1;
  WIDGET->cmdp=cmdp;
  return 0;
}

int ps_widget_sfxpoint_get_time(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return 0;
  return WIDGET->time;
}

double ps_widget_sfxpoint_get_value(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return 0.0;
  return WIDGET->value;
}

int ps_widget_sfxpoint_get_cmdp(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxpoint)) return 0;
  return WIDGET->cmdp;
}

/* Remove this point from the model, and self from the view.
 */
 
static int ps_sfxpoint_remove_self(struct ps_widget *widget) {

  /* First remove point from the model -- easy. */
  struct ps_iwg *iwg=ps_sfxpoint_get_iwg(widget);
  if (!iwg) return -1;
  if ((WIDGET->cmdp<0)||(WIDGET->cmdp>=iwg->cmdc)) return -1;
  if (ps_iwg_remove_commands(iwg,WIDGET->cmdp,1)<0) return -1;
  iwg->dirty=1;

  /* This invalidates all the graphs, so we must signal the editor to rebuild. */
  struct ps_widget *editsoundeffect=widget;
  while (editsoundeffect&&(editsoundeffect->type!=&ps_widget_type_editsoundeffect)) {
    editsoundeffect=editsoundeffect->parent;
  }
  if (!editsoundeffect) return -1;

  if (ps_widget_editsoundeffect_rebuild_graphs(editsoundeffect)<0) return -1;
  
  return 0;
}
