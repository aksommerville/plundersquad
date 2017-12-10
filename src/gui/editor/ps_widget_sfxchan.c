/* ps_widget_sfxchan.c
 * One channel in the sound effects editor.
 *
 * Children:
 *   [0] shape button
 *   [1] step limit button
 *   [2] trim limit button
 *   [3] step graph
 *   [4] trim graph
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "resedit/ps_iwg.h"
#include "akau/akau_wavegen.h"

#define PS_SFXCHAN_BGCOLOR_EVEN   0x0080c080
#define PS_SFXCHAN_BGCOLOR_ODD    0x00a0e080

#define PS_SFXCHAN_DEFAULT_HEIGHT 100
#define PS_SFXCHAN_HEADER_WIDTH 60

static int ps_sfxchan_cb_edit_shape(struct ps_widget *button,struct ps_widget *widget);
static int ps_sfxchan_cb_edit_step_limit(struct ps_widget *button,struct ps_widget *widget);
static int ps_sfxchan_cb_edit_trim_limit(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_sfxchan {
  struct ps_widget hdr;
  int chanid;
};

#define WIDGET ((struct ps_widget_sfxchan*)widget)

/* Delete.
 */

static void _ps_sfxchan_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_sfxchan_init(struct ps_widget *widget) {
  struct ps_widget *child;

  WIDGET->chanid=-1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // shape
  if (ps_widget_button_set_icon(child,0x0105)<0) return -1;
  if (ps_widget_button_set_margins(child,0,0)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_sfxchan_cb_edit_shape,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // step limit
  if (ps_widget_button_set_text(child,"2000",4)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_sfxchan_cb_edit_step_limit,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // trim limit
  if (ps_widget_button_set_text(child,"255",3)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_sfxchan_cb_edit_trim_limit,0,widget))<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_sfxgraph))) return -1; // step
  child->bgrgba=0xff000080;
  if (ps_widget_sfxgraph_set_key(child,AKAU_WAVEGEN_K_STEP)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_sfxgraph))) return -1; // trim
  child->bgrgba=0x00ff0080;
  if (ps_widget_sfxgraph_set_key(child,AKAU_WAVEGEN_K_TRIM)<0) return -1;

  return 0;
}

/* Get iwg from parent.
 */

static struct ps_iwg *ps_sfxchan_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  widget=widget->parent; // scrolllist
  if (!widget) return 0;
  widget=widget->parent; // editsoundeffect
  return ps_widget_editsoundeffect_get_iwg(widget);
}

/* Get child widgets.
 */

static struct ps_widget *ps_sfxchan_get_shape_button(const struct ps_widget *widget) {
  if (widget->childc!=5) return 0;
  return widget->childv[0];
}

static struct ps_widget *ps_sfxchan_get_step_limit_button(const struct ps_widget *widget) {
  if (widget->childc!=5) return 0;
  return widget->childv[1];
}

static struct ps_widget *ps_sfxchan_get_trim_limit_button(const struct ps_widget *widget) {
  if (widget->childc!=5) return 0;
  return widget->childv[2];
}

static struct ps_widget *ps_sfxchan_get_step_graph(const struct ps_widget *widget) {
  if (widget->childc!=5) return 0;
  return widget->childv[3];
}

static struct ps_widget *ps_sfxchan_get_trim_graph(const struct ps_widget *widget) {
  if (widget->childc!=5) return 0;
  return widget->childv[4];
}

/* Draw.
 */

static int _ps_sfxchan_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_sfxchan_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SCREENW;
  *h=PS_SFXCHAN_DEFAULT_HEIGHT;
  return 0;
}

/* Pack.
 */

static void ps_sfxchan_zero_child_sizes(struct ps_widget *widget) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[0];
    child->x=child->y=0;
    child->w=widget->w;
    child->h=widget->h;
  }
}

static int _ps_sfxchan_pack(struct ps_widget *widget) {

  /* Acquire child widgets. */
  if (widget->childc!=5) return -1;
  struct ps_widget *shapebutton,*steplimitbutton,*trimlimitbutton,*stepgraph,*trimgraph;
  if (!(shapebutton=ps_sfxchan_get_shape_button(widget))) return -1;
  if (!(steplimitbutton=ps_sfxchan_get_step_limit_button(widget))) return -1;
  if (!(trimlimitbutton=ps_sfxchan_get_trim_limit_button(widget))) return -1;
  if (!(stepgraph=ps_sfxchan_get_step_graph(widget))) return -1;
  if (!(trimgraph=ps_sfxchan_get_trim_graph(widget))) return -1;

  /* Panic if we're too narrow. */
  if (widget->w<=PS_SFXCHAN_HEADER_WIDTH) {
    ps_sfxchan_zero_child_sizes(widget);

  /* Establish child sizes. */
  } else {

    /* Limit buttons take half of the height, and the width of the longer, aligned right with header edge. 
     */
    int steplimitbuttonw,trimlimitbuttonw;
    if (ps_widget_measure(&steplimitbuttonw,0,steplimitbutton,PS_SFXCHAN_HEADER_WIDTH,widget->h>>1)<0) return -1;
    if (ps_widget_measure(&trimlimitbuttonw,0,trimlimitbutton,PS_SFXCHAN_HEADER_WIDTH,widget->h>>1)<0) return -1;
    int buttonw=(steplimitbuttonw>trimlimitbuttonw)?steplimitbuttonw:trimlimitbuttonw;
    steplimitbutton->x=trimlimitbutton->x=PS_SFXCHAN_HEADER_WIDTH-buttonw;
    steplimitbutton->w=trimlimitbutton->w=buttonw;
    steplimitbutton->y=0;
    steplimitbutton->h=widget->h>>1;
    trimlimitbutton->y=steplimitbutton->h;
    trimlimitbutton->h=widget->h-trimlimitbutton->y;

    /* Shape button is constant size, centered in the remaining header space.
     */
    shapebutton->w=PS_TILESIZE;
    shapebutton->h=PS_TILESIZE;
    shapebutton->x=(steplimitbutton->x>>1)-(shapebutton->w>>1);
    shapebutton->y=(widget->h>>1)-(shapebutton->h>>1);

    /* Graphs take all right of header, and each half of the height. Step on top.
     */
    stepgraph->x=trimgraph->x=PS_SFXCHAN_HEADER_WIDTH;
    stepgraph->w=trimgraph->w=widget->w-PS_SFXCHAN_HEADER_WIDTH;
    stepgraph->y=0;
    stepgraph->h=widget->h>>1;
    trimgraph->y=stepgraph->h;
    trimgraph->h=widget->h-stepgraph->h;
    
  }

  /* Pack everything. */
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_sfxchan={

  .name="sfxchan",
  .objlen=sizeof(struct ps_widget_sfxchan),

  .del=_ps_sfxchan_del,
  .init=_ps_sfxchan_init,

  .draw=_ps_sfxchan_draw,
  .measure=_ps_sfxchan_measure,
  .pack=_ps_sfxchan_pack,

};

/* Accessors.
 */
 
int ps_widget_sfxchan_get_chanid(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxchan)) return -1;
  return WIDGET->chanid;
}

/* Setup.
 */
 
int ps_widget_sfxchan_setup(struct ps_widget *widget,int chanid) {
  if (!widget||(widget->type!=&ps_widget_type_sfxchan)) return -1;
  struct ps_widget *stepgraph=ps_sfxchan_get_step_graph(widget);
  struct ps_widget *trimgraph=ps_sfxchan_get_trim_graph(widget);
  
  WIDGET->chanid=chanid;
  if (chanid&1) {
    widget->bgrgba=PS_SFXCHAN_BGCOLOR_ODD;
  } else {
    widget->bgrgba=PS_SFXCHAN_BGCOLOR_EVEN;
  }

  //TODO coordinate graph value limits with button in channel widget
  if (ps_widget_sfxgraph_set_value_limit(stepgraph,2000.0)<0) return -1;
  if (ps_widget_sfxgraph_set_value_limit(trimgraph,1.0)<0) return -1;

  if (ps_widget_sfxgraph_rebuild(stepgraph)<0) return -1;
  if (ps_widget_sfxgraph_rebuild(trimgraph)<0) return -1;

  return 0;
}

/* Button callbacks.
 */
 
static int ps_sfxchan_cb_edit_shape(struct ps_widget *button,struct ps_widget *widget) {
  //TODO
  return 0;
}

static int ps_sfxchan_cb_step_limit(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  if (!widget||(widget->type!=&ps_widget_type_sfxchan)) return -1;
  
  int nvalue;
  if (ps_widget_dialogue_get_number(&nvalue,dialogue)<0) return 0;
  if ((nvalue<0)||(nvalue>20000)) return 0;
  if (ps_widget_kill(dialogue)<0) return -1;
  char buf[32];
  int bufc=snprintf(buf,sizeof(buf),"%d",nvalue);
  if (ps_widget_button_set_text(ps_sfxchan_get_step_limit_button(widget),buf,bufc)<0) return -1;

  struct ps_widget *graph=ps_sfxchan_get_step_graph(widget);
  if (ps_widget_sfxgraph_set_value_limit(graph,nvalue)<0) return -1;
  if (ps_widget_pack(graph)<0) return -1;
  
  return 0;
}

static int ps_sfxchan_cb_edit_step_limit(struct ps_widget *button,struct ps_widget *widget) {
  const char *text=ps_widget_button_get_text(button);
  struct ps_widget *dialogue=ps_widget_spawn_dialogue_text(
    ps_widget_get_root(widget),
    "Step limit (Hz):",-1,text,-1,
    ps_sfxchan_cb_step_limit
  );
  if (!dialogue) return -1;
  if (ps_widget_ref(widget)<0) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,(void*)ps_widget_del)<0) return -1;
  return 0;
}

static int ps_sfxchan_cb_trim_limit(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  if (!widget||(widget->type!=&ps_widget_type_sfxchan)) return -1;

  int nvalue;
  if (ps_widget_dialogue_get_number(&nvalue,dialogue)<0) return 0;
  if ((nvalue<1)||(nvalue>255)) return 0;
  if (ps_widget_kill(dialogue)<0) return -1;
  char buf[32];
  int bufc=snprintf(buf,sizeof(buf),"%d",nvalue);
  if (ps_widget_button_set_text(ps_sfxchan_get_trim_limit_button(widget),buf,bufc)<0) return -1;

  struct ps_widget *graph=ps_sfxchan_get_trim_graph(widget);
  if (ps_widget_sfxgraph_set_value_limit(graph,nvalue/255.0)<0) return -1;
  if (ps_widget_pack(graph)<0) return -1;
  
  return 0;
}

static int ps_sfxchan_cb_edit_trim_limit(struct ps_widget *button,struct ps_widget *widget) {
  const char *text=ps_widget_button_get_text(button);
  struct ps_widget *dialogue=ps_widget_spawn_dialogue_text(
    ps_widget_get_root(widget),
    "Trim limit (1..255):",-1,text,-1,
    ps_sfxchan_cb_trim_limit
  );
  if (!dialogue) return -1;
  if (ps_widget_ref(widget)<0) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,(void*)ps_widget_del)<0) return -1;
  return 0;
}

/* Rebuild graphs.
 */
 
int ps_widget_sfxchan_rebuild_graphs(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_sfxchan)) return -1;
  struct ps_widget *stepgraph=ps_sfxchan_get_step_graph(widget);
  struct ps_widget *trimgraph=ps_sfxchan_get_trim_graph(widget);
  if (ps_widget_sfxgraph_rebuild(stepgraph)<0) return -1;
  if (ps_widget_sfxgraph_rebuild(trimgraph)<0) return -1;
  return 0;
}
