/* ps_widget_introtitle.c
 * Displays the main banner title on assemblepage.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

/* You can modify the tile IDs here and the vertex allocation will adapt.
 */
#define PS_INTROTITLE_TSID    0x01
#define PS_INTROTITLE_TILE_NW 0xb3
#define PS_INTROTITLE_TILE_SE 0xcc

/* Don't touch these unless you've substantially changed the layout.
 */
#define PS_INTROTITLE_COLC ((PS_INTROTITLE_TILE_SE&15)-(PS_INTROTITLE_TILE_NW&15)+1)
#define PS_INTROTITLE_ROWC ((PS_INTROTITLE_TILE_SE>>4)-(PS_INTROTITLE_TILE_NW>>4)+1)
#define PS_INTROTITLE_VTXC (PS_INTROTITLE_COLC*PS_INTROTITLE_ROWC)

/* Object definition.
 */

struct ps_widget_introtitle {
  struct ps_widget hdr;
  struct akgl_vtx_mintile vtxv[PS_INTROTITLE_VTXC];
};

#define WIDGET ((struct ps_widget_introtitle*)widget)

/* Draw.
 */

static void ps_introtitle_offset_vertices(struct ps_widget *widget,int dx,int dy) {
  struct akgl_vtx_mintile *vtx=WIDGET->vtxv;
  int i=PS_INTROTITLE_VTXC; for (;i-->0;vtx++) {
    vtx->x+=dx;
    vtx->y+=dy;
  }
}

static int _ps_introtitle_draw(struct ps_widget *widget,int parentx,int parenty) {
  ps_introtitle_offset_vertices(widget,widget->x+parentx,widget->y+parenty);
  int err=ps_video_draw_mintile(WIDGET->vtxv,PS_INTROTITLE_VTXC,PS_INTROTITLE_TSID);
  ps_introtitle_offset_vertices(widget,-widget->x-parentx,-widget->y-parenty);
  return 0;
}

/* Measure.
 */

static int _ps_introtitle_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_INTROTITLE_COLC*PS_TILESIZE;
  *h=PS_INTROTITLE_ROWC*PS_TILESIZE;
  return 0;
}

/* Pack.
 * Initialize vertices with the correct tile ID, and a position assuming widget at (0,0).
 */

static int _ps_introtitle_pack(struct ps_widget *widget) {

  int imgw=PS_INTROTITLE_COLC*PS_TILESIZE;
  int imgh=PS_INTROTITLE_ROWC*PS_TILESIZE;
  int imgx=(widget->w>>1)-(imgw>>1); // very left edge of tiles
  int imgy=(widget->h>>1)-(imgh>>1); // very top edge of tiles
  int x0=imgx+(PS_TILESIZE>>1); // center of top-left tile
  int y0=imgy+(PS_TILESIZE>>1); // "

  int y=y0;
  struct akgl_vtx_mintile *vtx=WIDGET->vtxv;
  int row=0; for (;row<PS_INTROTITLE_ROWC;row++,y+=PS_TILESIZE) {
    int x=x0;
    uint8_t tileid=PS_INTROTITLE_TILE_NW+(row<<4);
    int col=0; for (;col<PS_INTROTITLE_COLC;col++,vtx++,x+=PS_TILESIZE,tileid++) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=tileid;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_introtitle={

  .name="introtitle",
  .objlen=sizeof(struct ps_widget_introtitle),

  .draw=_ps_introtitle_draw,
  .measure=_ps_introtitle_measure,
  .pack=_ps_introtitle_pack,

};
