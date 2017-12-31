/* ps_widget_tileselect.c
 * Modal dialogue to select tile from tilesheet.
 * Tilesheets are 16x16 but the screen is 25x14, so we can't display the sheet in its native shape.
 * We'll remove the bottom 3 rows and display them in 4 columns to the right of the main sheet.
 * We handle the whole interaction; no child widgets.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"

#define PS_TILESELECT_MARGIN 5
#define PS_TILESELECT_MAIN_ROWC 13
#define PS_TILESELECT_AUX_COLC 4

#define PS_TILESELECT_ANIMATION_OFF_TIME   30
#define PS_TILESELECT_ANIMATION_PEAK_TIME  45
#define PS_TILESELECT_ANIMATION_LENGTH     60
#define PS_TILESELECT_PEAK_ALPHA 0xc0

/* Object definition.
 */

struct ps_widget_tileselect {
  struct ps_widget hdr;
  uint8_t tsid,tileid;
  struct akgl_vtx_mintile vtxv[256]; // We always need exactly 256 vertices, how convenient.
  int animp;
  int mousex,mousey;
  int clicktile;
  struct ps_callback cb;
};

#define WIDGET ((struct ps_widget_tileselect*)widget)

/* Delete.
 */

static void _ps_tileselect_del(struct ps_widget *widget) {
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_tileselect_init(struct ps_widget *widget) {

  widget->accept_mouse_focus=1;
  widget->drag_verbatim=1;
  widget->bgrgba=0xe0e0e0ff;
  WIDGET->clicktile=-1;

  return 0;
}

/* Get tile position.
 */

static void ps_tileselect_get_tile_position(int *x,int *y,const struct ps_widget *widget,uint8_t tileid) {
  int col=tileid%16;
  int row=tileid/16;
  if (row<PS_TILESELECT_MAIN_ROWC) {
    *x=PS_TILESELECT_MARGIN+col*PS_TILESIZE+(PS_TILESIZE>>1);
    *y=PS_TILESELECT_MARGIN+row*PS_TILESIZE+(PS_TILESIZE>>1);
  } else {
    tileid-=PS_TILESELECT_MAIN_ROWC*16;
    col=tileid%PS_TILESELECT_AUX_COLC;
    row=tileid/PS_TILESELECT_AUX_COLC;
    *x=(PS_TILESELECT_MARGIN<<1)+(PS_TILESIZE*16)+col*PS_TILESIZE+(PS_TILESIZE>>1);
    *y=PS_TILESELECT_MARGIN+row*PS_TILESIZE+(PS_TILESIZE>>1);
  }
}

static int ps_tileselect_tile_from_point(const struct ps_widget *widget,int x,int y) {
  if (x<PS_TILESELECT_MARGIN) return -1;
  if (y<PS_TILESELECT_MARGIN) return -1;
  int col=(x-PS_TILESELECT_MARGIN)/PS_TILESIZE;
  int row=(y-PS_TILESELECT_MARGIN)/PS_TILESIZE;
  if ((col<16)&&(row<PS_TILESELECT_MAIN_ROWC)) return (row*16)+col;
  int auxx=(PS_TILESELECT_MARGIN*2)+(PS_TILESIZE*16);
  if (x<auxx) return -1;
  col=(x-auxx)/PS_TILESIZE;
  if (col>=PS_TILESELECT_AUX_COLC) return -1;
  int tile=(PS_TILESELECT_MAIN_ROWC*16)+(row*PS_TILESELECT_AUX_COLC)+col;
  if (tile>=256) return -1;
  return tile;
}

/* Draw.
 */

static int _ps_tileselect_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  parentx+=widget->x;
  parenty+=widget->y;

  int i;
  struct akgl_vtx_mintile *vtx=WIDGET->vtxv;
  for (i=0;i<256;i++,vtx++) {
    vtx->x+=parentx;
    vtx->y+=parenty;
  }
  if (ps_video_draw_mintile(WIDGET->vtxv,256,WIDGET->tsid)<0) return -1;
  for (i=0,vtx=WIDGET->vtxv;i<256;i++,vtx++) {
    vtx->x-=parentx;
    vtx->y-=parenty;
  }

  WIDGET->animp++;
  if (WIDGET->animp>=PS_TILESELECT_ANIMATION_LENGTH) WIDGET->animp=0;
  if (WIDGET->animp>=PS_TILESELECT_ANIMATION_OFF_TIME) {
    int alpha;
    if (WIDGET->animp<PS_TILESELECT_ANIMATION_PEAK_TIME) {
      alpha=((WIDGET->animp-PS_TILESELECT_ANIMATION_OFF_TIME)*PS_TILESELECT_PEAK_ALPHA)/(PS_TILESELECT_ANIMATION_PEAK_TIME-PS_TILESELECT_ANIMATION_OFF_TIME);
    } else {
      alpha=((PS_TILESELECT_ANIMATION_LENGTH-WIDGET->animp)*PS_TILESELECT_PEAK_ALPHA)/(PS_TILESELECT_ANIMATION_LENGTH-PS_TILESELECT_ANIMATION_PEAK_TIME);
    }
    if (alpha<0) alpha=0;
    uint32_t highlight_color=0xff000000|alpha;
    int x,y;
    ps_tileselect_get_tile_position(&x,&y,widget,WIDGET->tileid);
    x+=parentx;
    y+=parenty;
    if (ps_video_draw_rect(x-(PS_TILESIZE>>1),y-(PS_TILESIZE>>1),PS_TILESIZE,PS_TILESIZE,highlight_color)<0) return -1;
  }
  
  return 0;
}

/* Measure.
 */

static int _ps_tileselect_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=(PS_TILESELECT_MARGIN*3)+(16*PS_TILESIZE)+(PS_TILESELECT_AUX_COLC*PS_TILESIZE);
  *h=(PS_TILESELECT_MARGIN*2)+(PS_TILESELECT_MAIN_ROWC*PS_TILESIZE);
  return 0;
}

/* Pack: Rewrite vtxv
 */

static int _ps_tileselect_pack(struct ps_widget *widget) {
  struct akgl_vtx_mintile *vtx=WIDGET->vtxv;
  int i=0; for (;i<256;i++,vtx++) {
    int x,y;
    ps_tileselect_get_tile_position(&x,&y,widget,i);
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=i;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_tileselect_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  return 0;
}

static int _ps_tileselect_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    int tile=ps_tileselect_tile_from_point(widget,WIDGET->mousex,WIDGET->mousey);
    if (value) {
      WIDGET->clicktile=tile;
    } else {
      if ((tile>=0)&&(tile==WIDGET->clicktile)) {
        WIDGET->tileid=tile;
        if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
        return ps_widget_kill(widget);
      }
      WIDGET->clicktile=-1;
    }
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_tileselect={

  .name="tileselect",
  .objlen=sizeof(struct ps_widget_tileselect),

  .del=_ps_tileselect_del,
  .init=_ps_tileselect_init,

  .draw=_ps_tileselect_draw,
  .measure=_ps_tileselect_measure,
  .pack=_ps_tileselect_pack,

  .mousemotion=_ps_tileselect_mousemotion,
  .mousebutton=_ps_tileselect_mousebutton,

};

/* Accessors.
 */
 
int ps_widget_tileselect_set_tileid(struct ps_widget *widget,uint16_t tileid) {
  if (!widget||(widget->type!=&ps_widget_type_tileselect)) return -1;
  WIDGET->tsid=tileid>>8;
  WIDGET->tileid=tileid;
  return 0;
}

uint16_t ps_widget_tileselect_get_tileid(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_tileselect)) return 0;
  return (WIDGET->tsid<<8)|WIDGET->tileid;
}
 
int ps_widget_tileselect_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (!widget||(widget->type!=&ps_widget_type_tileselect)) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}
