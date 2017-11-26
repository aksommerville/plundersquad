/* ps_widget_editsfxchan.c
 * Represents one channel in the sound effects editor.
 * Parent is editsfx.
 * Children:
 *   [0] editsfxgraph: step
 *   [1] editsfxgraph: trim
 */

#include "ps.h"
#include "resedit/ps_iwg.h"
#include "gui/ps_gui.h"
#include "video/ps_video.h"
#include "akau/akau.h"
#include "akgl/akgl.h"

// Metadata area on left side.
#define PS_EDITSFXCHAN_HEADER_WIDTH  (PS_TILESIZE<<1)
#define PS_EDITSFXCHAN_STEP_BGCOLOR    0x0080c080
#define PS_EDITSFXCHAN_TRIM_BGCOLOR    0x00c08080
#define PS_EDITSFXCHAN_STEP_FGCOLOR    0xffff00ff
#define PS_EDITSFXCHAN_TRIM_FGCOLOR    0xff00ffff

static int ps_editsfxchan_change_shape(struct ps_widget *widget);

/* Widget definition.
 */

struct ps_widget_editsfxchan {
  struct ps_widget hdr;
  int chanid;
  uint8_t shape_tileid;
};

#define WIDGET ((struct ps_widget_editsfxchan*)widget)

/* Get wavegen model from parent.
 */

static struct ps_iwg *ps_editsfxchan_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  return ps_widget_editsfx_get_iwg(widget->parent);
}

/* Delete.
 */

static void _ps_editsfxchan_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_editsfxchan_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_editsfxgraph))) return -1;
  child->bgrgba=PS_EDITSFXCHAN_STEP_BGCOLOR;
  child->fgrgba=PS_EDITSFXCHAN_STEP_FGCOLOR;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_editsfxgraph))) return -1;
  child->bgrgba=PS_EDITSFXCHAN_TRIM_BGCOLOR;
  child->fgrgba=PS_EDITSFXCHAN_TRIM_FGCOLOR;

  widget->track_mouse=1;
  WIDGET->shape_tileid=0x04;
  
  return 0;
}

/* Draw shape tile.
 */

static int ps_editsfxchan_draw_shape_tile(struct ps_widget *widget,int x0,int y0) {
  struct akgl_vtx_mintile vtx={
    .x=x0+widget->x+(PS_EDITSFXCHAN_HEADER_WIDTH>>1),
    .y=y0+widget->y+(widget->h>>1),
    .tileid=WIDGET->shape_tileid,
  };
  if (ps_video_draw_mintile(&vtx,1,1)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_editsfxchan_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  if (ps_editsfxchan_draw_shape_tile(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 * Doesn't matter; parent takes care of it.
 */

static int _ps_editsfxchan_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_editsfxchan_pack(struct ps_widget *widget) {

  if (widget->childc!=2) return -1;
  struct ps_widget *step=widget->childv[0];
  struct ps_widget *trim=widget->childv[1];
  
  int midy=widget->h>>1;
  int xleft=PS_EDITSFXCHAN_HEADER_WIDTH;

  step->x=xleft;
  step->w=widget->w-xleft;
  step->y=0;
  step->h=midy;
  if (ps_widget_pack(step)<0) return -1;

  trim->x=xleft;
  trim->w=widget->w-xleft;
  trim->y=midy;
  trim->h=widget->h-midy;
  if (ps_widget_pack(trim)<0) return -1;
  
  return 0;
}

/* Mouse events.
 */

static int _ps_editsfxchan_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfxchan_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfxchan_mousedown(struct ps_widget *widget,int btnid) {
  if (ps_editsfxchan_change_shape(widget)<0) return -1;
  return 0;
}

static int _ps_editsfxchan_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_editsfxchan_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsfxchan={
  .name="editsfxchan",
  .objlen=sizeof(struct ps_widget_editsfxchan),
  .del=_ps_editsfxchan_del,
  .init=_ps_editsfxchan_init,
  .draw=_ps_editsfxchan_draw,
  .measure=_ps_editsfxchan_measure,
  .pack=_ps_editsfxchan_pack,
  .mouseenter=_ps_editsfxchan_mouseenter,
  .mouseexit=_ps_editsfxchan_mouseexit,
  .mousedown=_ps_editsfxchan_mousedown,
  .mouseup=_ps_editsfxchan_mouseup,
  .mousewheel=_ps_editsfxchan_mousewheel,
};

/* Set channel ID.
 */
 
int ps_widget_editsfxchan_set_chanid(struct ps_widget *widget,int chanid) {
  if (!widget||(widget->type!=&ps_widget_type_editsfxchan)) return -1;
  if (chanid<0) return -1;
  struct ps_iwg *iwg=ps_editsfxchan_get_iwg(widget);
  if (!iwg) return -1;
  if (chanid>=iwg->chanc) return -1;

  WIDGET->chanid=chanid;
  const struct ps_iwg_channel *chan=iwg->chanv+chanid;
       if ((chan->argc==4)&&!memcmp(chan->arg,"sine",4)) WIDGET->shape_tileid=0x00;
  else if ((chan->argc==6)&&!memcmp(chan->arg,"square",6)) WIDGET->shape_tileid=0x01;
  else if ((chan->argc==3)&&!memcmp(chan->arg,"saw",3)) WIDGET->shape_tileid=0x02;
  else if ((chan->argc==5)&&!memcmp(chan->arg,"noise",5)) WIDGET->shape_tileid=0x03;
  else WIDGET->shape_tileid=0x04;
  
  if (widget->childc>=2) {
    if (ps_widget_editsfxgraph_set_field(widget->childv[0],WIDGET->chanid,AKAU_WAVEGEN_K_STEP)<0) return -1;
    if (ps_widget_editsfxgraph_set_field(widget->childv[1],WIDGET->chanid,AKAU_WAVEGEN_K_TRIM)<0) return -1;
  }
  
  return 0;
}

/* Change shape.
 */
 
static int ps_editsfxchan_change_shape(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsfxchan)) return -1;
  struct ps_iwg *iwg=ps_editsfxchan_get_iwg(widget);
  if (!iwg) return -1;
  if ((WIDGET->chanid<0)||(WIDGET->chanid>=iwg->chanc)) return -1;

  struct ps_iwg_channel *chan=iwg->chanv+WIDGET->chanid;  
  if ((chan->argc==4)&&!memcmp(chan->arg,"sine",4)) {
    if (ps_iwg_set_channel_shape(iwg,WIDGET->chanid,"square",6)<0) return -1;
    WIDGET->shape_tileid=0x01;
  } else if ((chan->argc==6)&&!memcmp(chan->arg,"square",6)) {
    if (ps_iwg_set_channel_shape(iwg,WIDGET->chanid,"saw",3)<0) return -1;
    WIDGET->shape_tileid=0x02;
  } else if ((chan->argc==3)&&!memcmp(chan->arg,"saw",3)) {
    if (ps_iwg_set_channel_shape(iwg,WIDGET->chanid,"noise",5)<0) return -1;
    WIDGET->shape_tileid=0x03;
  } else if ((chan->argc==5)&&!memcmp(chan->arg,"noise",5)) {
    if (ps_iwg_set_channel_shape(iwg,WIDGET->chanid,"sine",4)<0) return -1;
    WIDGET->shape_tileid=0x00;
  } else {
    if (ps_iwg_set_channel_shape(iwg,WIDGET->chanid,"sine",4)<0) return -1;
    WIDGET->shape_tileid=0x00;
  }
  iwg->dirty=1;
  
  return 0;
}
