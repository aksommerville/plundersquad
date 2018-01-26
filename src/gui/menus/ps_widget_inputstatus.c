/* ps_widget_inputstatus.c
 * Shows current state of an input device.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_map.h"
#include "input/ps_input_premap.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* Object definition.
 */

struct ps_widget_inputstatus {
  struct ps_widget hdr;
  struct ps_input_device *device;
  uint16_t device_buttons; // Updated constantly from device events.
  uint16_t manual_buttons; // Set by our owner.
  int use_device_buttons; // Nonzero to display the device state instead of manual (default=1)
  int watchid;
  struct akgl_vtx_maxtile vtxv[7];
};

#define WIDGET ((struct ps_widget_inputstatus*)widget)

/* Delete.
 */

static void _ps_inputstatus_del(struct ps_widget *widget) {
  if (WIDGET->device) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
    ps_input_device_del(WIDGET->device);
  }
}

/* Initialize.
 */

static int _ps_inputstatus_init(struct ps_widget *widget) {

  WIDGET->watchid=-1;
  WIDGET->use_device_buttons=1;

  return 0;
}

/* Structural helpers.
 */

static int ps_inputstatus_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_inputstatus) return -1;
  return 0;
}

/* Draw.
 */

static int ps_inputstatus_prepare_vertices(struct ps_widget *widget,int parentx,int parenty) {

  /* Set up the first vertex with common features and copy to all. */
  struct akgl_vtx_maxtile *vtx=WIDGET->vtxv;
  vtx->x=parentx+widget->x;
  vtx->y=parenty+widget->y;
  vtx->size=PS_TILESIZE;
  vtx->ta=0;
  vtx->pr=vtx->pg=vtx->pb=0x80;
  vtx->a=0xff;
  vtx->t=0;
  vtx->xform=AKGL_XFORM_NONE;
  int i=1; for (;i<7;i++) memcpy(WIDGET->vtxv+i,vtx,sizeof(struct akgl_vtx_maxtile));

  /* Set tile and transform for each vertex. */
  vtx[0].tileid=0x09; vtx[0].xform=AKGL_XFORM_NONE;
  vtx[1].tileid=0x09; vtx[1].xform=AKGL_XFORM_FLIP;
  vtx[2].tileid=0x09; vtx[2].xform=AKGL_XFORM_270;
  vtx[3].tileid=0x09; vtx[3].xform=AKGL_XFORM_90;
  vtx[4].tileid=0x0a;
  vtx[5].tileid=0x0b;
  vtx[6].tileid=0x0c;

  /* Set relative positions.
   * For now at least, keep the whole structure packed tight, centered in available space. (TODO revisit after seeing it)
   */
  int midx=widget->w>>1;
  int midy=widget->h>>1;
  int row0=midy-(PS_TILESIZE>>1);
  int row1=row0+PS_TILESIZE;
  int dpadx=midx-PS_TILESIZE; // horizontal center of dpad
  vtx[0].x+=dpadx; vtx[0].y+=row0; // UP
  vtx[1].x+=dpadx; vtx[1].y+=row1; // DOWN
  vtx[2].x+=dpadx-(PS_TILESIZE>>1); vtx[2].y+=midy; // LEFT
  vtx[3].x=vtx[2].x+PS_TILESIZE; vtx[3].y+=midy; // RIGHT
  vtx[4].x=vtx[2].x+PS_TILESIZE*3; vtx[4].y+=row1; // A (right)
  vtx[5].x=vtx[4].x-PS_TILESIZE; vtx[5].y+=row1; // B (left)
  vtx[6].x=vtx[5].x+(PS_TILESIZE>>1); vtx[6].y+=row0; // PAUSE (above A/B, centered between them)

  /* Set colors. */
  uint16_t state;
  uint8_t r,g,b;
  if (WIDGET->use_device_buttons) {
    state=WIDGET->device_buttons;
    r=0xff; g=0xff; b=0x00;
  } else {
    state=WIDGET->manual_buttons;
    r=0xff; g=0x00; b=0x00;
  }
  if (state) {
    if (state&PS_PLRBTN_UP) { vtx[0].pr=r;  vtx[0].pg=g; vtx[0].pb=b; }
    if (state&PS_PLRBTN_DOWN) { vtx[1].pr=r;  vtx[1].pg=g; vtx[1].pb=b; }
    if (state&PS_PLRBTN_LEFT) { vtx[2].pr=r;  vtx[2].pg=g; vtx[2].pb=b; }
    if (state&PS_PLRBTN_RIGHT) { vtx[3].pr=r;  vtx[3].pg=g; vtx[3].pb=b; }
    if (state&PS_PLRBTN_A) { vtx[4].pr=r;  vtx[4].pg=g; vtx[4].pb=b; }
    if (state&PS_PLRBTN_B) { vtx[5].pr=r;  vtx[5].pg=g; vtx[5].pb=b; }
    if (state&PS_PLRBTN_START) { vtx[6].pr=r;  vtx[6].pg=g; vtx[6].pb=b; }
  }

  return 0;
}

static int _ps_inputstatus_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  if (ps_inputstatus_prepare_vertices(widget,parentx,parenty)<0) return -1;
  if (ps_video_draw_maxtile(WIDGET->vtxv,7,1)<0) return -1;
  
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_inputstatus_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=200;
  *h=50;
  return 0;
}

/* Pack.
 */

static int _ps_inputstatus_pack(struct ps_widget *widget) {
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_inputstatus_update(struct ps_widget *widget) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_inputstatus={

  .name="inputstatus",
  .objlen=sizeof(struct ps_widget_inputstatus),

  .del=_ps_inputstatus_del,
  .init=_ps_inputstatus_init,

  .draw=_ps_inputstatus_draw,
  .measure=_ps_inputstatus_measure,
  .pack=_ps_inputstatus_pack,

  .update=_ps_inputstatus_update,

};

/* Button callback.
 */

static int ps_inputstatus_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;
  //ps_log(GUI,TRACE,"%s %d=%d [%s]",__func__,btnid,value,mapped?"mapped":"raw");

  if (mapped) {
    if (value) {
      WIDGET->device_buttons|=btnid;
    } else {
      WIDGET->device_buttons&=~btnid;
    }
  }
  
  return 0;
}

/* Set device.
 */
 
int ps_widget_inputstatus_set_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (ps_inputstatus_obj_validate(widget)<0) return -1;
  if (device==WIDGET->device) return 0;

  if (WIDGET->device) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
    ps_input_device_del(WIDGET->device);
    WIDGET->device=0;
    WIDGET->watchid=-1;
  }

  if (device) {
    if (ps_input_device_ref(device)<0) return -1;
    WIDGET->device=device;
    if ((WIDGET->watchid=ps_input_device_watch_buttons(device,ps_inputstatus_cb_button,0,widget))<0) return -1;
  }
  
  return 0;
}

/* Manual button highlight.
 */
 
int ps_widget_inputstatus_set_manual(struct ps_widget *widget,int manual) {
  if (ps_inputstatus_obj_validate(widget)<0) return -1;
  WIDGET->use_device_buttons=manual?0:1;
  return 0;
}

int ps_widget_inputstatus_highlight(struct ps_widget *widget,uint16_t btnid,int value) {
  if (ps_inputstatus_obj_validate(widget)<0) return -1;
  if (value) {
    WIDGET->manual_buttons|=btnid;
  } else {
    WIDGET->manual_buttons&=~btnid;
  }
  return 0;
}
