/* ps_widget_sfxgraph.c
 * Graph view for one property of one channel in sound effects editor.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/editor/ps_editor.h"
#include "resedit/ps_iwg.h"
#include "akau/akau_wavegen.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_SFXGRAPH_LINE_COLOR 0xffffffff
#define PS_SFXGRAPH_POINT_SIZE 8

/* Object definition.
 */

struct ps_widget_sfxgraph {
  struct ps_widget hdr;
  int k;
  double valuez; // Range of y axis. We acquire x dynamically as needed.
  struct akgl_vtx_raw *vtxv;
  int vtxa;
  int mousex,mousey;
};

#define WIDGET ((struct ps_widget_sfxgraph*)widget)

/* Delete.
 */

static void _ps_sfxgraph_del(struct ps_widget *widget) {
  if (WIDGET->vtxv) free(WIDGET->vtxv);
}

/* Initialize.
 */

static int _ps_sfxgraph_init(struct ps_widget *widget) {

  WIDGET->k=AKAU_WAVEGEN_K_NOOP;
  WIDGET->valuez=100.0;
  WIDGET->mousex=-1; // initialize negative to ensure we do not react when mousedown comes before mousemotion
  WIDGET->mousey=-1;
  
  return 0;
}

/* Get iwg from grandparent.
 */

static struct ps_iwg *ps_sfxgraph_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  widget=widget->parent; // sfxchan
  if (!widget) return 0;
  widget=widget->parent; // scrolllist
  if (!widget) return 0;
  widget=widget->parent; // editsoundeffect
  return ps_widget_editsoundeffect_get_iwg(widget);
}

/* Draw lines.
 */

static int ps_sfxgraph_draw_lines(struct ps_widget *widget,int parentx,int parenty) {
  if (widget->childc<2) return 0;
  parentx+=widget->x;
  parenty+=widget->y;

  if (WIDGET->vtxa<widget->childc) {
    void *nv=realloc(WIDGET->vtxv,widget->childc*sizeof(struct akgl_vtx_raw));
    if (!nv) return -1;
    WIDGET->vtxv=nv;
    WIDGET->vtxa=widget->childc;
  }

  struct akgl_vtx_raw *vtx=WIDGET->vtxv;
  vtx->x=parentx+widget->childv[0]->x+(widget->childv[0]->w>>1);
  vtx->y=parenty+widget->childv[0]->y+(widget->childv[0]->h>>1);
  vtx->r=PS_SFXGRAPH_LINE_COLOR>>24;
  vtx->g=PS_SFXGRAPH_LINE_COLOR>>16;
  vtx->b=PS_SFXGRAPH_LINE_COLOR>>8;
  vtx->a=PS_SFXGRAPH_LINE_COLOR;
  vtx++;
  int i=1; for (;i<widget->childc;i++,vtx++) {
    memcpy(vtx,WIDGET->vtxv,sizeof(struct akgl_vtx_raw));
    struct ps_widget *child=widget->childv[i];
    vtx->x=parentx+child->x+(child->w>>1);
    vtx->y=parenty+child->y+(child->h>>1);
  }

  if (ps_video_draw_line_strip(WIDGET->vtxv,widget->childc)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_sfxgraph_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_video_flush_cached_drawing()<0) return -1;
  if (ps_sfxgraph_draw_lines(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_sfxgraph_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_sfxgraph_pack(struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  if ((widget->w<1)||(widget->h<1)) return 0;
  
  const struct ps_iwg *iwg=ps_sfxgraph_get_iwg(widget);
  if (!iwg||(iwg->cmdc<1)) return 0;
  int maxtime=iwg->cmdv[iwg->cmdc-1].time;
  if (maxtime<1) maxtime=widget->w;

  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];

    child->w=PS_SFXGRAPH_POINT_SIZE;
    child->h=PS_SFXGRAPH_POINT_SIZE;
    
    int time=ps_widget_sfxpoint_get_time(child);
    if (time<0) time=0;
    else if (time>maxtime) time=maxtime;
    child->x=(time*widget->w)/maxtime-(child->w>>1);
    
    double value=ps_widget_sfxpoint_get_value(child);
    if (value<0.0) value=0.0;
    else if (value>WIDGET->valuez) value=WIDGET->valuez;
    child->y=widget->h-(value*widget->h)/WIDGET->valuez-(child->h>>1);

    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_sfxgraph_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_sfxgraph_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  return 0;
}

static int _ps_sfxgraph_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if ((btnid==1)&&value) {
    if (ps_widget_sfxgraph_add_point(widget,WIDGET->mousex,WIDGET->mousey)<0) return -1;
  }
  return 0;
}

static int _ps_sfxgraph_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_sfxgraph_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

static int _ps_sfxgraph_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Digested input events.
 */

static int _ps_sfxgraph_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_sfxgraph_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_sfxgraph_activate(struct ps_widget *widget) {
  return 0;
}

static int _ps_sfxgraph_cancel(struct ps_widget *widget) {
  return 0;
}

static int _ps_sfxgraph_adjust(struct ps_widget *widget,int d) {
  return 0;
}

static int _ps_sfxgraph_focus(struct ps_widget *widget) {
  return 0;
}

static int _ps_sfxgraph_unfocus(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_sfxgraph={

  .name="sfxgraph",
  .objlen=sizeof(struct ps_widget_sfxgraph),

  .del=_ps_sfxgraph_del,
  .init=_ps_sfxgraph_init,

  .draw=_ps_sfxgraph_draw,
  .measure=_ps_sfxgraph_measure,
  .pack=_ps_sfxgraph_pack,

  .update=_ps_sfxgraph_update,
  .mousemotion=_ps_sfxgraph_mousemotion,
  .mousebutton=_ps_sfxgraph_mousebutton,
  .mousewheel=_ps_sfxgraph_mousewheel,
  .key=_ps_sfxgraph_key,
  .userinput=_ps_sfxgraph_userinput,

  .mouseenter=_ps_sfxgraph_mouseenter,
  .mouseexit=_ps_sfxgraph_mouseexit,
  .activate=_ps_sfxgraph_activate,
  .cancel=_ps_sfxgraph_cancel,
  .adjust=_ps_sfxgraph_adjust,
  .focus=_ps_sfxgraph_focus,
  .unfocus=_ps_sfxgraph_unfocus,

};

/* Set key.
 */
 
int ps_widget_sfxgraph_set_key(struct ps_widget *widget,int key) {
  if (!widget||(widget->type!=&ps_widget_type_sfxgraph)) return -1;
  switch (key) {
    case AKAU_WAVEGEN_K_STEP:
    case AKAU_WAVEGEN_K_TRIM:
      break;
    default: return -1;
  }
  WIDGET->k=key;
  return 0;
}

/* Limits.
 */
 
int ps_widget_sfxgraph_set_value_limit(struct ps_widget *widget,double v) {
  if (!widget||(widget->type!=&ps_widget_type_sfxgraph)) return -1;
  if (v<=0.0) return -1;
  WIDGET->valuez=v;
  return 0;
}

double ps_widget_sfxgraph_get_value_limit(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxgraph)) return 0.0;
  return WIDGET->valuez;
}

/* Rebuild UI.
 */
 
int ps_widget_sfxgraph_rebuild(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxgraph)) return -1;
  if (ps_widget_remove_all_children(widget)<0) return -1;
  int chanid=ps_widget_sfxchan_get_chanid(widget->parent);
  const struct ps_iwg *iwg=ps_sfxgraph_get_iwg(widget);
  if (iwg) {
    const struct ps_iwg_command *cmd=iwg->cmdv;
    int i=0; for (;i<iwg->cmdc;i++,cmd++) {
      if (cmd->chanid!=chanid) continue;
      if (cmd->k!=WIDGET->k) continue;
      struct ps_widget *point=ps_widget_spawn(widget,&ps_widget_type_sfxpoint);
      if (!point) return -1;
      if (ps_widget_sfxpoint_set_cmdp(point,i)<0) return -1;
      if (ps_widget_sfxpoint_set_time(point,cmd->time)<0) return -1;
      if (ps_widget_sfxpoint_set_value(point,cmd->v)<0) return -1;
    }
  }
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

/* Calculations for adding point.
 */

static int ps_sfxgraph_locate_insertion_point(const struct ps_widget *widget,int x) {
  int i=0; for (;i<widget->childc;i++) {
    const struct ps_widget *child=widget->childv[i];
    int childx=child->x+(child->w>>1);
    if (x<=childx) return i;
  }
  return widget->childc;
}

static int ps_sfxgraph_time_from_horz(const struct ps_widget *widget,const struct ps_iwg *iwg,int x) {

  if (widget->w<1) return -1;
  if (iwg->cmdc<1) return widget->w;
  int maxtime=iwg->cmdv[iwg->cmdc-1].time;
  if (maxtime<1) maxtime=widget->w;

  int time=(x*maxtime)/widget->w;
  if (time<0) return 0;
  if (time>maxtime) return maxtime;
  return time;
}

static double ps_sfxgraph_value_from_vert(const struct ps_widget *widget,int y) {
  if (WIDGET->valuez<1.0) return 0.0;
  if (widget->h<1) return 0.0;
  double v=((widget->h-y-1)*WIDGET->valuez)/widget->h;
  if (v<0.0) return 0.0;
  if (v>WIDGET->valuez) return WIDGET->valuez;
  return v;
}

/* Add point.
 */
 
int ps_widget_sfxgraph_add_point(struct ps_widget *widget,int x,int y) {
  if (!widget||(widget->type!=&ps_widget_type_sfxgraph)) return -1;
  
  if ((x<0)||(y<0)||(x>=widget->w)||(y>=widget->h)) {
    ps_log(EDIT,DEBUG,"Rejecting new point at (%d,%d) in sfxgraph size (%d,%d).",x,y,widget->w,widget->h);
    return 0;
  }

  struct ps_iwg *iwg=ps_sfxgraph_get_iwg(widget);
  if (!iwg) return -1;

  int childp=ps_sfxgraph_locate_insertion_point(widget,x);
  if (childp<0) return -1;
  int time=ps_sfxgraph_time_from_horz(widget,iwg,x);
  if (time<0) return -1;
  int chanid=ps_widget_sfxchan_get_chanid(widget->parent);
  if (chanid<0) return -1;
  double v=ps_sfxgraph_value_from_vert(widget,y);
  if (v<0.0) return -1;

  struct ps_widget *point=ps_widget_new(&ps_widget_type_sfxpoint);
  if (!point) return -1;
  if (ps_widget_insert_child(widget,childp,point)<0) {
    ps_widget_del(point);
    return -1;
  }
  ps_widget_del(point);

  int cmdp=ps_iwg_add_command(iwg,time,chanid,WIDGET->k,v);
  if (cmdp<0) return -1;
  iwg->dirty=1;
  if (ps_widget_sfxpoint_set_cmdp(point,cmdp)<0) return -1;
  if (ps_widget_sfxpoint_set_time(point,time)<0) return -1;
  if (ps_widget_sfxpoint_set_value(point,v)<0) return -1;

  /* This invalidates all the graphs, so we must signal the editor to rebuild. */
  struct ps_widget *editsoundeffect=widget;
  while (editsoundeffect&&(editsoundeffect->type!=&ps_widget_type_editsoundeffect)) {
    editsoundeffect=editsoundeffect->parent;
  }
  if (!editsoundeffect) return -1;

  if (ps_widget_editsoundeffect_rebuild_graphs(editsoundeffect)<0) return -1;

  return 0;
}
