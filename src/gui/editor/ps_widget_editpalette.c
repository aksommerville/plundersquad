/* ps_widget_editpalette.c
 * Modal dialogue for editing palette colors.
 * We draw a preview of the sprite ourselves.
 * RGB slider and preview are children.
 *
 * Children:
 *  [0] headpreview
 *  [1] headr
 *  [2] headg
 *  [3] headb
 *  [4] bodypreview
 *  [5] bodyr
 *  [6] bodyg
 *  [7] bodyb
 *  [8] okbutton
 *  [9] cancelbutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_EDITPALETTE_LEFT_SPACE 32 /* Reserved area on left side for sprite preview. */
#define PS_EDITPALETTE_MARGIN 5
#define PS_EDITPALETTE_PREVIEW_WIDTH 16

static int ps_editpalette_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_editpalette_cb_cancel(struct ps_widget *button,struct ps_widget *widget);
static int ps_editpalette_cb_rgbslider(struct ps_widget *slider,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editpalette {
  struct ps_widget hdr;
  struct akgl_vtx_maxtile vtxv[2];
  struct ps_callback cb;
  int refnum1;
};

#define WIDGET ((struct ps_widget_editpalette*)widget)

/* Delete.
 */

static void _ps_editpalette_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_editpalette_init(struct ps_widget *widget) {

  widget->bgrgba=0xc0c0c0ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_blotter))) return -1; // headpreview
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // headr
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,0)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // headg
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,1)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // headb
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,2)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_blotter))) return -1; // bodypreview
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // bodyr
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,0)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // bodyg
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,1)<0) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_rgbslider))) return -1; // bodyb
  if (ps_widget_rgbslider_set_callback(child,ps_callback(ps_editpalette_cb_rgbslider,0,widget))<0) return -1;
  if (ps_widget_rgbslider_set_channel(child,2)<0) return -1;
  
  if (!(child=ps_widget_button_spawn(widget,0,"OK",2,ps_callback(ps_editpalette_cb_ok,0,widget)))) return -1; // okbutton
  if (!(child=ps_widget_button_spawn(widget,0,"Cancel",6,ps_callback(ps_editpalette_cb_cancel,0,widget)))) return -1; // cancelbutton

  return 0;
}

/* Structural helpers.
 */

static int ps_editpalette_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editpalette) return -1;
  if (widget->childc!=10) return -1;
  return 0;
}

static struct ps_widget *ps_editpalette_get_headpreview(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_editpalette_get_headr(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_editpalette_get_headg(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_editpalette_get_headb(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_editpalette_get_bodypreview(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_editpalette_get_bodyr(const struct ps_widget *widget) { return widget->childv[5]; }
static struct ps_widget *ps_editpalette_get_bodyg(const struct ps_widget *widget) { return widget->childv[6]; }
static struct ps_widget *ps_editpalette_get_bodyb(const struct ps_widget *widget) { return widget->childv[7]; }
static struct ps_widget *ps_editpalette_get_okbutton(const struct ps_widget *widget) { return widget->childv[8]; }
static struct ps_widget *ps_editpalette_get_cancelbutton(const struct ps_widget *widget) { return widget->childv[9]; }

/* Draw.
 */

static int _ps_editpalette_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  /* Vertex tile IDs are set by our parent and don't change.
   * Everything else, we update here.
   */
  struct ps_widget *headpreview=ps_editpalette_get_headpreview(widget);
  struct ps_widget *bodypreview=ps_editpalette_get_bodypreview(widget);
  struct akgl_vtx_maxtile *vtxbody=WIDGET->vtxv;
  struct akgl_vtx_maxtile *vtxhead=WIDGET->vtxv+1;
  vtxbody->x=parentx+widget->x+(PS_EDITPALETTE_LEFT_SPACE>>1);
  vtxbody->y=parenty+widget->y+(widget->h>>1);
  vtxbody->size=PS_TILESIZE;
  vtxbody->ta=0;
  vtxbody->pr=bodypreview->bgrgba>>24;
  vtxbody->pg=bodypreview->bgrgba>>16;
  vtxbody->pb=bodypreview->bgrgba>>8;
  vtxbody->a=bodypreview->bgrgba;
  vtxbody->t=0;
  vtxbody->xform=AKGL_XFORM_NONE;
  vtxhead->x=vtxbody->x;
  vtxhead->y=vtxbody->y-12;
  vtxhead->size=PS_TILESIZE;
  vtxhead->ta=0;
  vtxhead->pr=headpreview->bgrgba>>24;
  vtxhead->pg=headpreview->bgrgba>>16;
  vtxhead->pb=headpreview->bgrgba>>8;
  vtxhead->a=headpreview->bgrgba;
  vtxhead->t=0;
  vtxhead->xform=AKGL_XFORM_NONE;
  if (ps_video_draw_maxtile(WIDGET->vtxv,2,0x03)<0) return -1;

  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_editpalette_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_SCREENW>>1;
  *h=PS_SCREENH>>1;
  return 0;
}

/* Pack.
 */

static int _ps_editpalette_pack(struct ps_widget *widget) {
  if (ps_editpalette_obj_validate(widget)<0) return -1;
  struct ps_widget *headpreview=ps_editpalette_get_headpreview(widget);
  struct ps_widget *headr=ps_editpalette_get_headr(widget);
  struct ps_widget *headg=ps_editpalette_get_headg(widget);
  struct ps_widget *headb=ps_editpalette_get_headb(widget);
  struct ps_widget *bodypreview=ps_editpalette_get_bodypreview(widget);
  struct ps_widget *bodyr=ps_editpalette_get_bodyr(widget);
  struct ps_widget *bodyg=ps_editpalette_get_bodyg(widget);
  struct ps_widget *bodyb=ps_editpalette_get_bodyb(widget);
  struct ps_widget *okbutton=ps_editpalette_get_okbutton(widget);
  struct ps_widget *cancelbutton=ps_editpalette_get_cancelbutton(widget);
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,okbutton,widget->w,widget->h)<0) return -1;
  okbutton->x=widget->w-PS_EDITPALETTE_MARGIN-chw;
  okbutton->y=widget->h-PS_EDITPALETTE_MARGIN-chh;
  okbutton->w=chw;
  okbutton->h=chh;

  if (ps_widget_measure(&chw,&chh,cancelbutton,widget->w,widget->h)<0) return -1;
  cancelbutton->x=okbutton->x-chw;
  cancelbutton->y=okbutton->y;
  cancelbutton->w=chw;
  cancelbutton->h=okbutton->h;

  int availh=okbutton->y-PS_EDITPALETTE_MARGIN;

  headpreview->x=PS_EDITPALETTE_LEFT_SPACE;
  headpreview->y=PS_EDITPALETTE_MARGIN;
  headpreview->w=PS_EDITPALETTE_PREVIEW_WIDTH;
  headpreview->h=(availh-PS_EDITPALETTE_MARGIN)>>1;

  bodypreview->x=headpreview->x;
  bodypreview->y=headpreview->y+headpreview->h+PS_EDITPALETTE_MARGIN;
  bodypreview->w=headpreview->w;
  bodypreview->h=okbutton->y-PS_EDITPALETTE_MARGIN-bodypreview->y;

  headr->x=headpreview->x+headpreview->w+PS_EDITPALETTE_MARGIN;
  headr->y=headpreview->y;
  headr->w=widget->w-PS_EDITPALETTE_MARGIN-headr->x;
  headr->h=(headpreview->h-(PS_EDITPALETTE_MARGIN<<1))/3;

  headg->x=headr->x;
  headg->y=headr->y+headr->h+PS_EDITPALETTE_MARGIN;
  headg->w=headr->w;
  headg->h=headr->h;

  headb->x=headr->x;
  headb->y=headg->y+headg->h+PS_EDITPALETTE_MARGIN;
  headb->w=headr->w;
  headb->h=headpreview->y+headpreview->h-headb->y;

  bodyr->x=bodypreview->x+bodypreview->w+PS_EDITPALETTE_MARGIN;
  bodyr->y=bodypreview->y;
  bodyr->w=widget->w-PS_EDITPALETTE_MARGIN-bodyr->x;
  bodyr->h=(bodypreview->h-(PS_EDITPALETTE_MARGIN<<1))/3;

  bodyg->x=bodyr->x;
  bodyg->y=bodyr->y+bodyr->h+PS_EDITPALETTE_MARGIN;
  bodyg->w=bodyr->w;
  bodyg->h=bodyr->h;

  bodyb->x=bodyr->x;
  bodyb->y=bodyg->y+bodyg->h+PS_EDITPALETTE_MARGIN;
  bodyb->w=bodyr->w;
  bodyb->h=bodypreview->y+bodypreview->h-bodyb->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editpalette={

  .name="editpalette",
  .objlen=sizeof(struct ps_widget_editpalette),

  .del=_ps_editpalette_del,
  .init=_ps_editpalette_init,

  .draw=_ps_editpalette_draw,
  .measure=_ps_editpalette_measure,
  .pack=_ps_editpalette_pack,

};

/* Callbacks.
 */
 
static int ps_editpalette_cb_ok(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editpalette_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_editpalette_cb_rgbslider(struct ps_widget *slider,struct ps_widget *widget) {
  uint32_t rgba=ps_widget_rgbslider_get_rgba(slider);
  if (ps_widget_get_index_of_child(widget,slider)<5) {
    ps_editpalette_get_headpreview(widget)->bgrgba=rgba;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headr(widget),rgba)<0) return -1;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headg(widget),rgba)<0) return -1;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headb(widget),rgba)<0) return -1;
  } else {
    ps_editpalette_get_bodypreview(widget)->bgrgba=rgba;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyr(widget),rgba)<0) return -1;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyg(widget),rgba)<0) return -1;
    if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyb(widget),rgba)<0) return -1;
  }
  return 0;
}

/* Accessors.
 */
 
int ps_widget_editpalette_setup(
  struct ps_widget *widget,
  uint8_t tileid_head,uint8_t tileid_body,
  uint32_t rgba_head,uint32_t rgba_body,
  struct ps_callback cb
) {
  if (ps_editpalette_obj_validate(widget)<0) return -1;
  
  WIDGET->vtxv[0].tileid=tileid_body;
  WIDGET->vtxv[1].tileid=tileid_head;
  
  ps_editpalette_get_headpreview(widget)->bgrgba=rgba_head;
  ps_editpalette_get_bodypreview(widget)->bgrgba=rgba_body;

  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headr(widget),rgba_head)<0) return -1;
  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headg(widget),rgba_head)<0) return -1;
  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_headb(widget),rgba_head)<0) return -1;
  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyr(widget),rgba_body)<0) return -1;
  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyg(widget),rgba_body)<0) return -1;
  if (ps_widget_rgbslider_set_rgba(ps_editpalette_get_bodyb(widget),rgba_body)<0) return -1;
  
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;

  return 0;
}

uint32_t ps_widget_editpalette_get_rgba_head(const struct ps_widget *widget) {
  if (ps_editpalette_obj_validate(widget)<0) return 0;
  return ps_editpalette_get_headpreview(widget)->bgrgba;
}

uint32_t ps_widget_editpalette_get_rgba_body(const struct ps_widget *widget) {
  if (ps_editpalette_obj_validate(widget)<0) return 0;
  return ps_editpalette_get_bodypreview(widget)->bgrgba;
}

int ps_widget_editpalette_set_refnum1(struct ps_widget *widget,int v) {
  if (ps_editpalette_obj_validate(widget)<0) return -1;
  WIDGET->refnum1=v;
  return 0;
}

int ps_widget_editpalette_get_refnum1(const struct ps_widget *widget) {
  if (ps_editpalette_obj_validate(widget)<0) return -1;
  return WIDGET->refnum1;
}
