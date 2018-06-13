#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "video/ps_video.h"
#include "util/ps_linebreaker.h"

/* Way more than can fit on a screen, assuming the usual 6x12 glyphs. */
#define PS_TEXTBLOCK_SANITY_LIMIT 2047

/* Object definition.
 */

struct ps_widget_textblock {
  struct ps_widget hdr;
  char *text;
  int textc;
  struct ps_linebreaker lines;
  int font_resid;
  int size;
  int horzalign,vertalign;
};

#define WIDGET ((struct ps_widget_textblock*)widget)

/* Delete.
 */

static void _ps_textblock_del(struct ps_widget *widget) {
  if (WIDGET->text) free(WIDGET->text);
  ps_linebreaker_cleanup(&WIDGET->lines);
}

/* Initialize.
 */

static int _ps_textblock_init(struct ps_widget *widget) {
  widget->bgrgba=0x00000000;
  widget->fgrgba=0x000000ff;
  WIDGET->font_resid=0;
  WIDGET->size=12;
  WIDGET->horzalign=-1;
  WIDGET->vertalign=-1;
  return 0;
}

/* Draw.
 */

static int _ps_textblock_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (WIDGET->textc&&WIDGET->lines.c) {
    if (ps_video_text_begin()<0) return -1;

    int y;
    if (WIDGET->vertalign<0) {
      y=parenty+widget->y+(WIDGET->size>>1);
    } else if (WIDGET->vertalign>0) {
      y=parenty+widget->y+widget->h-(WIDGET->size>>1)-((WIDGET->lines.c-1)*WIDGET->size);
    } else {
      int texth=WIDGET->size*WIDGET->lines.c;
      int top=parenty+widget->y+(widget->h>>1)-(texth>>1);
      y=top+(WIDGET->size>>1);
    }
    
    int i=WIDGET->lines.c;
    const struct ps_linebreaker_line *line=WIDGET->lines.v;
    for (;i-->0;line++) {
    
      int x;
      if (WIDGET->horzalign<0) {
        x=parentx+widget->x+(WIDGET->size>>2);
      } else if (WIDGET->horzalign>0) {
        x=parentx+widget->x+widget->w-((WIDGET->size>>1)*line->c)+(WIDGET->size>>2);
      } else {
        int textw=line->c*(WIDGET->size>>1);
        int left=parentx+widget->x+(widget->w>>1)-(textw>>1);
        x=left+(WIDGET->size>>2);
      }
      
      if (ps_video_text_add(WIDGET->size,widget->fgrgba,x,y,WIDGET->text+line->p,line->c)<0) return -1;
      y+=WIDGET->size;
    }
    if (ps_video_text_end(WIDGET->font_resid)<0) return -1;
  }
  return 0;
}

/* Measure.
 */

static int _ps_textblock_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {

  /* Break lines on the maximum width. */
  if (ps_linebreaker_rebuild(&WIDGET->lines,WIDGET->text,WIDGET->textc,WIDGET->size>>1,maxw)<0) return -1;

  /* Height is a simple multiple of the line count. */
  *h=WIDGET->lines.c*WIDGET->size;

  /* Width is based on the longest line, not counting trailing space. */
  int longestline=0;
  int i=WIDGET->lines.c;
  const struct ps_linebreaker_line *line=WIDGET->lines.v;
  for (;i-->0;line++) {
    const char *src=WIDGET->text+line->p;
    int srcc=line->c;
    while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
    if (srcc>longestline) longestline=srcc;
  }
  *w=(longestline*WIDGET->size)>>1;
  //(*w)+=WIDGET->size>>1; // Add one character's width because the line breaker is not perfect.

  return 0;
}

/* Pack.
 * We never have children, but we do need to rebreak lines if our width changes.
 */

static int _ps_textblock_pack(struct ps_widget *widget) {
  if (ps_linebreaker_rebuild(&WIDGET->lines,WIDGET->text,WIDGET->textc,WIDGET->size>>1,widget->w)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_textblock={

  .name="textblock",
  .objlen=sizeof(struct ps_widget_textblock),

  .del=_ps_textblock_del,
  .init=_ps_textblock_init,

  .draw=_ps_textblock_draw,
  .measure=_ps_textblock_measure,
  .pack=_ps_textblock_pack,

};

/* Set text.
 */
 
int ps_widget_textblock_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_textblock)) return -1;

  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>PS_TEXTBLOCK_SANITY_LIMIT) srcc=PS_TEXTBLOCK_SANITY_LIMIT;
  
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (WIDGET->text) free(WIDGET->text);
  WIDGET->text=nv;
  WIDGET->textc=srcc;

  if (ps_linebreaker_rebuild(&WIDGET->lines,WIDGET->text,WIDGET->textc,WIDGET->size>>1,widget->w)<0) return -1;
  
  return 0;
}

/* Set alignment.
 */
 
int ps_widget_textblock_set_alignment(struct ps_widget *widget,int horz,int vert) {
  if (!widget||(widget->type!=&ps_widget_type_textblock)) return -1;
  WIDGET->horzalign=(horz<0)?-1:(horz>0)?1:0;
  WIDGET->vertalign=(vert<0)?-1:(vert>0)?1:0;
  return 0;
}
