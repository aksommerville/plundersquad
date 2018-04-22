/* ps_widget_bpchart.c
 * Visual blueprint layout editor.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "scenario/ps_blueprint.h"

/* Object definition.
 */

struct ps_widget_bpchart {
  struct ps_widget hdr;
  int x0,y0; // Start drawing at this offset, within my bounds.
  int colw,rowh;
  struct akgl_vtx_mintile *vtxv;
  int vtxc,vtxa;
  int mousex,mousey;
  int mousecol,mouserow;
  int dragpoip; // If >=0, we are dragging that POI.
  int painting; // If nonzero, we are overwriting cells.
  uint8_t paintbrush;
};

#define WIDGET ((struct ps_widget_bpchart*)widget)

/* Delete.
 */

static void _ps_bpchart_del(struct ps_widget *widget) {
  if (WIDGET->vtxv) free(WIDGET->vtxv);
}

/* Initialize.
 */

static int _ps_bpchart_init(struct ps_widget *widget) {

  widget->bgrgba=0x000000ff;
  widget->accept_mouse_focus=1;
  widget->accept_mouse_wheel=1;
  widget->drag_verbatim=1;

  WIDGET->mousex=-1;
  WIDGET->mousey=-1;
  WIDGET->mousecol=-1;
  WIDGET->mouserow=-1;
  WIDGET->dragpoip=-1;
  
  return 0;
}

/* Draw lines across columns or rows.
 */

static int ps_bpchart_draw_vertical_lines(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x+WIDGET->x0+WIDGET->colw;
  parenty+=widget->y+WIDGET->y0;
  int lineh=WIDGET->rowh*PS_BLUEPRINT_ROWC;
  int i=1; for (;i<PS_BLUEPRINT_COLC;i++,parentx+=WIDGET->colw) {
    if (ps_video_draw_rect(parentx,parenty,1,lineh,0x000000ff)<0) return -1;
  }
  return 0;
}

static int ps_bpchart_draw_horizontal_lines(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x+WIDGET->x0;
  parenty+=widget->y+WIDGET->y0+WIDGET->rowh;
  int linew=WIDGET->colw*PS_BLUEPRINT_COLC;
  int i=1; for (;i<PS_BLUEPRINT_ROWC;i++,parenty+=WIDGET->rowh) {
    if (ps_video_draw_rect(parentx,parenty,linew,1,0x000000ff)<0) return -1;
  }
  return 0;
}

/* Colors.
 * 00..06 are used.
 */

static uint32_t ps_bpchart_color_for_cell(uint8_t cell) {
  switch (cell&7) {
    case 0: return 0x60a070ff; // VACANT
    case 1: return 0x804020ff; // SOLID
    case 2: return 0x0000ffff; // HOLE
    case 3: return 0x406000ff; // LATCH
    case 4: return 0x00ffffff; // HEROONLY
    case 5: return 0xff0000ff; // HAZARD
    case 6: return 0x00ff00ff; // HEAL
    case 7: return 0xffff00ff; // STATUSREPORT
  }
  return 0x000000ff;
}

/* Grow mintile vertex buffer.
 */

static struct akgl_vtx_mintile *ps_bpchart_vtx_new(struct ps_widget *widget) {
  if (WIDGET->vtxc>=WIDGET->vtxa) {
    int na=WIDGET->vtxa+16;
    if (na>INT_MAX/sizeof(struct akgl_vtx_mintile)) return 0;
    void *nv=realloc(WIDGET->vtxv,sizeof(struct akgl_vtx_mintile)*na);
    if (!nv) return 0;
    WIDGET->vtxv=nv;
    WIDGET->vtxa=na;
  }
  return WIDGET->vtxv+WIDGET->vtxc++;
}

/* Draw POI indicator.
 */

static int ps_bpchart_draw_poi(struct ps_widget *widget,int x,int y,uint8_t type,const int *argv) {
  struct akgl_vtx_mintile *vtx=ps_bpchart_vtx_new(widget);
  if (!vtx) return -1;
  vtx->x=x+6;
  vtx->y=y+6;
  if (type<0x0f) {
    vtx->tileid=0x20+type;
  } else {
    vtx->tileid=0x2f;
  }
  return 0;
}

/* Draw.
 */

static int _ps_bpchart_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  // Unpacked, or too small. Abort.
  if ((WIDGET->colw<1)||(WIDGET->rowh<1)) return 0;
  
  const struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
  if (blueprint) {

    /* Draw cells. */
    const uint8_t *cell=blueprint->cellv;
    int y=parenty+widget->y+WIDGET->y0;
    int row=0; for (;row<PS_BLUEPRINT_ROWC;row++,y+=WIDGET->rowh) {
      int x=parentx+widget->x+WIDGET->x0;
      int col=0; for (;col<PS_BLUEPRINT_COLC;col++,cell++,x+=WIDGET->colw) {
        ps_video_draw_rect(x,y,WIDGET->colw,WIDGET->rowh,ps_bpchart_color_for_cell(*cell));
      }
    }
    if (ps_video_flush_cached_drawing()<0) return -1;

    /* Draw lines if wide enough. */
    if (WIDGET->colw>=4) ps_bpchart_draw_vertical_lines(widget,parentx,parenty);
    if (WIDGET->rowh>=4) ps_bpchart_draw_horizontal_lines(widget,parentx,parenty);
    if (ps_video_flush_cached_drawing()<0) return -1;

    /* Draw POI. */
    if (blueprint->poic>0) {
      WIDGET->vtxc=0;
      int basex=parentx+widget->x+WIDGET->x0;
      int basey=parenty+widget->y+WIDGET->y0;
      const struct ps_blueprint_poi *poi=blueprint->poiv;
      int i=blueprint->poic; for (;i-->0;poi++) {
        if (ps_bpchart_draw_poi(widget,basex+poi->x*WIDGET->colw,basey+poi->y*WIDGET->rowh,poi->type,poi->argv)<0) return -1;
      }
      if (ps_video_draw_mintile(WIDGET->vtxv,WIDGET->vtxc,0x01)<0) return -1;
    }
    
  }
  return 0;
}

/* Measure.
 */

static int _ps_bpchart_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_TILESIZE*PS_BLUEPRINT_COLC;
  *h=PS_TILESIZE*PS_BLUEPRINT_ROWC;
  return 0;
}

/* Pack.
 */

static int _ps_bpchart_pack(struct ps_widget *widget) {
  WIDGET->colw=widget->w/PS_BLUEPRINT_COLC;
  WIDGET->rowh=widget->h/PS_BLUEPRINT_ROWC;
  if (WIDGET->colw<1) WIDGET->colw=1;
  if (WIDGET->rowh<1) WIDGET->rowh=1;
  if (WIDGET->colw>WIDGET->rowh) WIDGET->colw=WIDGET->rowh;
  else WIDGET->rowh=WIDGET->colw;
  WIDGET->x0=(widget->w-WIDGET->colw*PS_BLUEPRINT_COLC)>>1;
  WIDGET->y0=(widget->h-WIDGET->rowh*PS_BLUEPRINT_ROWC)>>1;
  return 0;
}

/* Modifications by dragging.
 */

static int ps_bpchart_drag_poi_to_cell(struct ps_widget *widget,int poip,int col,int row) {
  struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
  if (!blueprint) return -1;
  if ((poip<0)||(poip>=blueprint->poic)) return -1;
  if (col<0) col=0; else if (col>=PS_BLUEPRINT_COLC) col=PS_BLUEPRINT_COLC-1;
  if (row<0) row=0; else if (row>=PS_BLUEPRINT_ROWC) row=PS_BLUEPRINT_ROWC-1;
  blueprint->poiv[poip].x=col;
  blueprint->poiv[poip].y=row;
  return 0;
}

static int ps_bpchart_paint_cell(struct ps_widget *widget,int col,int row,uint8_t cell) {
  struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
  if (!blueprint) return -1;
  if (col<0) col=0; else if (col>=PS_BLUEPRINT_COLC) col=PS_BLUEPRINT_COLC-1;
  if (row<0) row=0; else if (row>=PS_BLUEPRINT_ROWC) row=PS_BLUEPRINT_ROWC-1;
  blueprint->cellv[row*PS_BLUEPRINT_COLC+col]=cell;
  return 0;
}

/* Mouse motion.
 */

static int _ps_bpchart_mousemotion(struct ps_widget *widget,int x,int y) {

  WIDGET->mousex=x;
  WIDGET->mousey=y;

  int mousecol,mouserow;
  if (x<WIDGET->x0) mousecol=-1;
  else mousecol=(x-WIDGET->x0)/WIDGET->colw;
  if (y<WIDGET->y0) mouserow=-1;
  else mouserow=(y-WIDGET->y0)/WIDGET->rowh;

  if ((mousecol!=WIDGET->mousecol)||(mouserow!=WIDGET->mouserow)) {
    WIDGET->mousecol=mousecol;
    WIDGET->mouserow=mouserow;
    if (WIDGET->dragpoip>=0) {
      if (ps_bpchart_drag_poi_to_cell(widget,WIDGET->dragpoip,mousecol,mouserow)<0) return -1;
    } else if (WIDGET->painting) {
      if (ps_bpchart_paint_cell(widget,mousecol,mouserow,WIDGET->paintbrush)<0) return -1;
    } else {
      if (ps_widget_editblueprint_highlight_poi_for_cell(widget,mousecol,mouserow)<0) return -1;
    }
  }
  
  return 0;
}

static int ps_bpchart_force_mousemotion(struct ps_widget *widget) {
  WIDGET->mousecol=WIDGET->mouserow=-1;
  return _ps_bpchart_mousemotion(widget,WIDGET->mousex,WIDGET->mousey);
}

/* Mouse button.
 */

static int ps_bpchart_find_poi_at_cell(const struct ps_widget *widget,int col,int row) {
  const struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
  if (!blueprint) return -1;
  int i=blueprint->poic; while (i-->0) {
    const struct ps_blueprint_poi *poi=blueprint->poiv+i;
    if (poi->x!=col) continue;
    if (poi->y!=row) continue;
    return i;
  }
  return -1;
}

static int ps_bpchart_begin_drag_or_paint(struct ps_widget *widget) {
  if ((WIDGET->mousecol<0)||(WIDGET->mousecol>=PS_BLUEPRINT_COLC)) return 0;
  if ((WIDGET->mouserow<0)||(WIDGET->mouserow>=PS_BLUEPRINT_ROWC)) return 0;
  if (ps_widget_editblueprint_highlight_poi_for_cell(widget,-1,-1)<0) return -1;
  int subx=(WIDGET->mousex-WIDGET->x0)%WIDGET->colw;
  int suby=(WIDGET->mousey-WIDGET->y0)%WIDGET->rowh;
  if ((subx<12)&&(suby<12)) {
    if ((WIDGET->dragpoip=ps_bpchart_find_poi_at_cell(widget,WIDGET->mousecol,WIDGET->mouserow))>=0) {
      return 0;
    }
  }
  WIDGET->painting=1;
  if (ps_bpchart_force_mousemotion(widget)<0) return -1;
  return 0;
}

static int ps_bpchart_pick_up_color(struct ps_widget *widget) {
  if (WIDGET->mousecol<0) return 0;
  if (WIDGET->mouserow<0) return 0;
  if (WIDGET->mousecol>=PS_BLUEPRINT_COLC) return 0;
  if (WIDGET->mouserow>=PS_BLUEPRINT_ROWC) return 0;
  const struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
  if (!blueprint) return -1;
  WIDGET->paintbrush=blueprint->cellv[WIDGET->mouserow*PS_BLUEPRINT_COLC+WIDGET->mousecol];
  return 0;
}

static int _ps_bpchart_mousebutton(struct ps_widget *widget,int btnid,int value) {
  if (btnid==1) {
    if (value) {
      if (ps_bpchart_begin_drag_or_paint(widget)<0) return -1;
    } else {
      WIDGET->dragpoip=-1;
      WIDGET->painting=0;
      if (ps_widget_editblueprint_highlight_poi_for_cell(widget,WIDGET->mousecol,WIDGET->mouserow)<0) return -1;
    }
  } else if (btnid==3) {
    if (value) {
      if (ps_bpchart_pick_up_color(widget)<0) return -1;
    }
  }
  return 0;
}

/* Other input.
 */

static int _ps_bpchart_mousewheel(struct ps_widget *widget,int dx,int dy) {
  if ((WIDGET->mousecol>=0)&&(WIDGET->mousecol<PS_BLUEPRINT_COLC)) {
    if ((WIDGET->mouserow>=0)&&(WIDGET->mouserow<PS_BLUEPRINT_ROWC)) {
      struct ps_blueprint *blueprint=ps_widget_editblueprint_get_blueprint(widget);
      if (!blueprint) return 0;
      uint8_t *cell=blueprint->cellv+(WIDGET->mouserow*PS_BLUEPRINT_COLC)+WIDGET->mousecol;
      if ((dx>0)||(dy>0)) {
        if (++(*cell)>7) *cell=0;
      } else {
        if (*cell==0) *cell=7; else (*cell)--;
      }
    }
  }
  return 0;
}

static int _ps_bpchart_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_bpchart={

  .name="bpchart",
  .objlen=sizeof(struct ps_widget_bpchart),

  .del=_ps_bpchart_del,
  .init=_ps_bpchart_init,

  .draw=_ps_bpchart_draw,
  .measure=_ps_bpchart_measure,
  .pack=_ps_bpchart_pack,

  .mousemotion=_ps_bpchart_mousemotion,
  .mousebutton=_ps_bpchart_mousebutton,
  .mousewheel=_ps_bpchart_mousewheel,
  .key=_ps_bpchart_key,

};
