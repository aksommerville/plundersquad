/* ps_widget_regionshape.c
 * List item detailing one region shape. Interactive.
 *
 * Children:
 *  [0] deletebutton
 *  [1] physicsbutton
 *  [2] weightbutton
 *  [3] tilebutton
 *  [4] stylebutton
 *  [5] roundtoggle
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "scenario/ps_region.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"
#include "util/ps_enums.h"
#include "util/ps_text.h"

#define PS_REGIONSHAPE_MARGIN 5
#define PS_REGIONSHAPE_ROW_HEIGHT 16
#define PS_REGIONSHAPE_MINIMUM_HEIGHT (PS_REGIONSHAPE_MARGIN*3+PS_REGIONSHAPE_ROW_HEIGHT*2)

static int ps_regionshape_cb_physics(struct ps_widget *button,struct ps_widget *widget);
static int ps_regionshape_cb_weight(struct ps_widget *button,struct ps_widget *widget);
static int ps_regionshape_cb_tile(struct ps_widget *button,struct ps_widget *widget);
static int ps_regionshape_cb_style(struct ps_widget *button,struct ps_widget *widget);
static int ps_regionshape_cb_round(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_regionshape {
  struct ps_widget hdr;
  struct ps_region_shape shape;
  uint8_t tsid;
  struct akgl_vtx_mintile vtxv[16]; // Largest preview is 4x4.
};

#define WIDGET ((struct ps_widget_regionshape*)widget)

/* Delete.
 */

static void _ps_regionshape_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_regionshape_init(struct ps_widget *widget) {
  struct ps_widget *child;

  if (!(child=ps_widget_button_spawn(widget,0,"X",1,ps_callback(0,0,0)))) return -1; // deletebutton (callback set later by owner)
  child->bgrgba=0x800000ff;

  if (!(child=ps_widget_button_spawn(widget,0,"PHYSICS",7,ps_callback(ps_regionshape_cb_physics,0,widget)))) return -1; // physicsbutton
  if (!(child=ps_widget_button_spawn(widget,0,"WEIGHT",6,ps_callback(ps_regionshape_cb_weight,0,widget)))) return -1; // weightbutton
  if (!(child=ps_widget_button_spawn(widget,0,"TILE",4,ps_callback(ps_regionshape_cb_tile,0,widget)))) return -1; // tilebutton
  if (!(child=ps_widget_button_spawn(widget,0,"STYLE",5,ps_callback(ps_regionshape_cb_style,0,widget)))) return -1; // stylebutton
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_toggle))) return -1; // roundtoggle
  if (ps_widget_toggle_set_text(child,"O",1)<0) return -1;
  if (ps_widget_toggle_set_callback(child,ps_callback(ps_regionshape_cb_round,0,widget))<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_regionshape_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_regionshape) return -1;
  if (widget->childc!=6) return -1;
  return 0;
}

static struct ps_widget *ps_regionshape_get_deletebutton(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_regionshape_get_physicsbutton(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_regionshape_get_weightbutton(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_regionshape_get_tilebutton(const struct ps_widget *widget) { return widget->childv[3]; }
static struct ps_widget *ps_regionshape_get_stylebutton(const struct ps_widget *widget) { return widget->childv[4]; }
static struct ps_widget *ps_regionshape_get_roundtoggle(const struct ps_widget *widget) { return widget->childv[5]; }

/* Desscribe preview.
 */

static void ps_regionshape_get_preview_size(int *colc,int *rowc,uint8_t style) {
  switch (style) {
    case PS_REGION_SHAPE_STYLE_SINGLE: *colc=1; *rowc=1; break;
    case PS_REGION_SHAPE_STYLE_ALT4: *colc=2; *rowc=2; break;
    case PS_REGION_SHAPE_STYLE_ALT8: *colc=2; *rowc=4; break;
    case PS_REGION_SHAPE_STYLE_EVEN4: *colc=2; *rowc=2; break;
    case PS_REGION_SHAPE_STYLE_SKINNY: *colc=3; *rowc=3; break;
    case PS_REGION_SHAPE_STYLE_FAT: *colc=3; *rowc=3; break;
    case PS_REGION_SHAPE_STYLE_3X3: *colc=3; *rowc=3; break;
    case PS_REGION_SHAPE_STYLE_ALT16: *colc=4; *rowc=4; break;
    default: *colc=1; *rowc=1; break;
  }
}

// Add result to base tile.
static uint8_t ps_regionshape_get_preview_tile(uint8_t style,int col,int row) {
  switch (style) {
    case PS_REGION_SHAPE_STYLE_SINGLE: return 0x00;
    case PS_REGION_SHAPE_STYLE_ALT4: return (row*2)+col;
    case PS_REGION_SHAPE_STYLE_ALT8: return (col*16)+row;
    case PS_REGION_SHAPE_STYLE_EVEN4: return (row*2)+col;
    case PS_REGION_SHAPE_STYLE_SKINNY: switch (row*3+col) {
        case 0: return 0x00;
        case 1: return 0x01;
        case 2: return 0x02;
        case 3: return 0x10;
        case 4: return 0x11;
        case 5: return 0x22;
        case 6: return 0x20;
        case 7: return 0x22;
        case 8: return 0x33;
      } break;
    case PS_REGION_SHAPE_STYLE_FAT: switch (row*3+col) {
        case 0: return 0x00;
        case 1: return 0x01;
        case 2: return 0x02;
        case 3: return 0x10;
        case 4: return 0x03;
        case 5: return 0x22;
        case 6: return 0x20;
        case 7: return 0x22;
        case 8: return 0x23;
      } break;
    case PS_REGION_SHAPE_STYLE_3X3: return (row*16)+col;
    case PS_REGION_SHAPE_STYLE_ALT16: return (row*16)+col;
  }
  return 0x00;
}

/* Draw preview.
 */

static int ps_regionshape_draw_preview(struct ps_widget *widget,int parentx,int parenty) {
  int colc,rowc;
  ps_regionshape_get_preview_size(&colc,&rowc,WIDGET->shape.style);
  int dstx=parentx+widget->x+PS_REGIONSHAPE_MARGIN;
  int dsty=parenty+widget->y+(widget->h>>1)-((rowc*PS_TILESIZE)>>1);
  if (ps_video_draw_rect(dstx-1,dsty-1,colc*PS_TILESIZE+2,rowc*PS_TILESIZE+2,0x000000ff)<0) return -1;
  if (ps_video_flush_cached_drawing()<0) return -1;
  dstx+=PS_TILESIZE>>1;
  dsty+=PS_TILESIZE>>1;

  struct akgl_vtx_mintile *vtx=WIDGET->vtxv;
  int row=0; for (;row<rowc;row++) {
    int col=0; for (;col<colc;col++,vtx++) {
      vtx->x=dstx+col*PS_TILESIZE;
      vtx->y=dsty+row*PS_TILESIZE;
      vtx->tileid=WIDGET->shape.tileid+ps_regionshape_get_preview_tile(WIDGET->shape.style,col,row);
    }
  }
  if (ps_video_draw_mintile(WIDGET->vtxv,colc*rowc,WIDGET->tsid)<0) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_regionshape_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_regionshape_draw_preview(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 * Any width is fine, ask for the maximum.
 * For height, consider the preview size.
 */

static int _ps_regionshape_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  int chw,chh;
  *w=maxw;

  int previewcolc,previewrowc;
  ps_regionshape_get_preview_size(&previewcolc,&previewrowc,WIDGET->shape.style);
  *h=(previewrowc*PS_TILESIZE)+(PS_REGIONSHAPE_MARGIN*2);
  if (*h<PS_REGIONSHAPE_MINIMUM_HEIGHT) *h=PS_REGIONSHAPE_MINIMUM_HEIGHT;
  
  return 0;
}

/* Pack.
 * Children pack right-to-left from the top, breaking lines if we encroach on the preview.
 */

static int _ps_regionshape_pack(struct ps_widget *widget) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  int colc,rowc,leftlimit,chw,chh,i;
  ps_regionshape_get_preview_size(&colc,&rowc,WIDGET->shape.style);
  leftlimit=PS_REGIONSHAPE_MARGIN*2+colc*PS_TILESIZE;
  int y=PS_REGIONSHAPE_MARGIN;
  int x=widget->w-PS_REGIONSHAPE_MARGIN;
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;

    /* Break line if it exceeds the left margin AND it's not the first in the row. */
    if ((x!=widget->w-PS_REGIONSHAPE_MARGIN)&&(x-chw-PS_REGIONSHAPE_MARGIN<leftlimit)) {
      x=widget->w-PS_REGIONSHAPE_MARGIN;
      y+=PS_REGIONSHAPE_ROW_HEIGHT+PS_REGIONSHAPE_MARGIN;
    }

    x-=chw+PS_REGIONSHAPE_MARGIN;
    child->x=x;
    child->y=y;
    child->w=chw;
    child->h=PS_REGIONSHAPE_ROW_HEIGHT;
    
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_regionshape={

  .name="regionshape",
  .objlen=sizeof(struct ps_widget_regionshape),

  .del=_ps_regionshape_del,
  .init=_ps_regionshape_init,

  .draw=_ps_regionshape_draw,
  .measure=_ps_regionshape_measure,
  .pack=_ps_regionshape_pack,

};

/* Populate UI from model.
 */

static int ps_regionshape_populate_ui(struct ps_widget *widget) {
  struct ps_widget *physicsbutton=ps_regionshape_get_physicsbutton(widget);
  struct ps_widget *weightbutton=ps_regionshape_get_weightbutton(widget);
  struct ps_widget *tilebutton=ps_regionshape_get_tilebutton(widget);
  struct ps_widget *stylebutton=ps_regionshape_get_stylebutton(widget);
  struct ps_widget *roundtoggle=ps_regionshape_get_roundtoggle(widget);
  char buf[32];
  int bufc;

  if (ps_widget_button_set_text(physicsbutton,ps_blueprint_cell_repr(WIDGET->shape.physics),-1)<0) return -1;
  if (ps_widget_button_set_text(stylebutton,ps_region_shape_style_repr(WIDGET->shape.style),-1)<0) return -1;
  if (ps_widget_toggle_set_value(roundtoggle,WIDGET->shape.flags&PS_REGION_SHAPE_FLAG_ROUND)<0) return -1;

  bufc=ps_decsint_repr(buf,sizeof(buf),WIDGET->shape.weight);
  if (ps_widget_button_set_text(weightbutton,buf,bufc)<0) return -1;

  bufc=snprintf(buf,sizeof(buf),"0x%02x",WIDGET->shape.tileid);
  if (ps_widget_button_set_text(tilebutton,buf,bufc)<0) return -1;

  return 0;
}

/* Accessors.
 */
 
int ps_widget_regionshape_set_shape(struct ps_widget *widget,const struct ps_region_shape *shape) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  if (shape) {
    memcpy(&WIDGET->shape,shape,sizeof(struct ps_region_shape));
  } else {
    memset(&WIDGET->shape,0,sizeof(struct ps_region_shape));
  }
  if (ps_regionshape_populate_ui(widget)<0) return -1;
  return 0;
}

int ps_widget_regionshape_get_shape(struct ps_region_shape *shape,const struct ps_widget *widget) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  if (!shape) return -1;
  memcpy(shape,&WIDGET->shape,sizeof(struct ps_region_shape));
  return 0;
}

int ps_widget_regionshape_set_tsid(struct ps_widget *widget,uint8_t tsid) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  WIDGET->tsid=tsid;
  return 0;
}

int ps_widget_regionshape_set_cb_delete(struct ps_widget *widget,struct ps_callback cb_delete) {
  if (ps_regionshape_obj_validate(widget)<0) return -1;
  if (ps_widget_button_set_callback(ps_regionshape_get_deletebutton(widget),cb_delete)<0) return -1;
  return 0;
}

/* Edit physics.
 */

static int ps_regionshape_cb_physics_ok(struct ps_widget *scrolllist,struct ps_widget *widget) {
  int physics=ps_widget_scrolllist_get_selection(scrolllist);
  if ((physics>=0)&&(physics<=255)) {
    WIDGET->shape.physics=physics;
    if (ps_regionshape_populate_ui(widget)<0) return -1;
    if (ps_widget_pack(widget)<0) return -1;
  }
  if (ps_widget_plaindialogue_dismiss(scrolllist)<0) return -1;
  return 0;
}
 
static int ps_regionshape_cb_physics(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_plaindialogue);
  struct ps_widget *scrolllist=ps_widget_spawn(dialogue,&ps_widget_type_scrolllist);
  if (ps_widget_scrolllist_enable_selection(scrolllist,ps_callback(ps_regionshape_cb_physics_ok,0,widget))<0) return -1;

  int physics=0;
  for (;physics<256;physics++) {
    const char *name=ps_blueprint_cell_repr(physics);
    if (!name) break;
    if (!ps_widget_scrolllist_add_label(scrolllist,name,-1)) return -1;
  }
  if (ps_widget_scrolllist_set_selection_without_callback(scrolllist,WIDGET->shape.physics)<0) return -1;
  
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit weight.
 */

static int ps_regionshape_cb_weight_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int nweight;
  if (ps_widget_dialogue_get_number(&nweight,dialogue)<0) return 0;
  if ((nweight<0)||(nweight>255)) return 0;
  WIDGET->shape.weight=nweight;
  if (ps_regionshape_populate_ui(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_regionshape_cb_weight(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Weight:",-1,
    WIDGET->shape.weight,
    ps_regionshape_cb_weight_ok
  );
  if (!dialogue) return -1;
  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  return 0;
}

/* Edit tile.
 */

static int ps_regionshape_cb_tile_ok(struct ps_widget *tileselect,struct ps_widget *widget) {
  WIDGET->shape.tileid=ps_widget_tileselect_get_tileid(tileselect);
  return 0;
}

static int ps_regionshape_cb_tile(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *tileselect=ps_widget_spawn(root,&ps_widget_type_tileselect);
  if (ps_widget_tileselect_set_tileid(tileselect,(WIDGET->tsid<<8)|WIDGET->shape.tileid)<0) return -1;
  if (ps_widget_tileselect_set_callback(tileselect,ps_callback(ps_regionshape_cb_tile_ok,0,widget))<0) return -1;
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Edit style.
 */

static int ps_regionshape_cb_style_ok(struct ps_widget *scrolllist,struct ps_widget *widget) {
  int style=ps_widget_scrolllist_get_selection(scrolllist);
  if ((style>=0)&&(style<=255)) {
    WIDGET->shape.style=style;
    if (ps_regionshape_populate_ui(widget)<0) return -1;
    
    // Changing style could change our desired height, so pack my parent instead of myself.
    // And since our parent is a scrolllist, we have to measure before packing, to refresh sizes.
    if (ps_widget_measure(0,0,widget->parent,PS_SCREENW,PS_SCREENH)<0) return -1;
    if (ps_widget_pack(widget->parent)<0) return -1;
    
  }
  if (ps_widget_plaindialogue_dismiss(scrolllist)<0) return -1;
  return 0;
}

static int ps_regionshape_cb_style(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_plaindialogue);
  struct ps_widget *scrolllist=ps_widget_spawn(dialogue,&ps_widget_type_scrolllist);
  if (ps_widget_scrolllist_enable_selection(scrolllist,ps_callback(ps_regionshape_cb_style_ok,0,widget))<0) return -1;

  int style=0;
  for (;style<256;style++) {
    const char *name=ps_region_shape_style_repr(style);
    if (!name) break;
    if (!ps_widget_scrolllist_add_label(scrolllist,name,-1)) return -1;
  }
  if (ps_widget_scrolllist_set_selection_without_callback(scrolllist,WIDGET->shape.style)<0) return -1;
  
  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Toggle round flag.
 */

static int ps_regionshape_cb_round(struct ps_widget *roundtoggle,struct ps_widget *widget) {
  if (ps_widget_toggle_get_value(roundtoggle)) {
    WIDGET->shape.flags|=PS_REGION_SHAPE_FLAG_ROUND;
  } else {
    WIDGET->shape.flags&=~PS_REGION_SHAPE_FLAG_ROUND;
  }
  return 0;
}
