#include "ps.h"
#include "../ps_widget.h"
#include "ps_corewidgets.h"
#include "util/ps_buffer.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define PS_FIELD_DEFAULT_WIDTH 100
#define PS_FIELD_MARGIN 3
#define PS_FIELD_INSERTION_POINT_BLINK_TIME 30

/* Object definition.
 */

struct ps_widget_field {
  struct ps_widget hdr;
  int font_resid;
  int size;
  struct ps_buffer text;
  int insp;
  int insp_blink;
  int focus;
};

#define WIDGET ((struct ps_widget_field*)widget)

/* Delete.
 */

static void _ps_field_del(struct ps_widget *widget) {
  ps_buffer_cleanup(&WIDGET->text);
}

/* Initialize.
 */

static int _ps_field_init(struct ps_widget *widget) {

  widget->bgrgba=0xffffffff;
  widget->fgrgba=0x000000ff;
  WIDGET->font_resid=0;
  WIDGET->size=12;
  
  return 0;
}

/* Draw border.
 */

static int ps_field_draw_border(struct ps_widget *widget,int parentx,int parenty) {
  uint8_t r=0x00,g=0x00,b=0x00,a=0xff;
  struct akgl_vtx_raw vtxv[5]={
    {parentx,parenty,r,g,b,a},
    {parentx+widget->w,parenty,r,g,b,a},
    {parentx+widget->w,parenty+widget->h,r,g,b,a},
    {parentx,parenty+widget->h,r,g,b,a},
    {parentx,parenty,r,g,b,a},
  };
  if (ps_video_draw_line_strip(vtxv,5)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_field_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  
  int textx=parentx+widget->x+(WIDGET->size>>2)+PS_FIELD_MARGIN;
  int texty=parenty+widget->y+(widget->h>>1)+1;

  if (WIDGET->text.c>0) {
    if (ps_video_text_begin()<0) return -1;
    if (ps_video_text_add(WIDGET->size,widget->fgrgba,textx,texty,WIDGET->text.v,WIDGET->text.c)<0) return -1;
    if (ps_video_text_end(WIDGET->font_resid)<0) return -1;
  }

  if (WIDGET->focus) {
    if (++(WIDGET->insp_blink)>=PS_FIELD_INSERTION_POINT_BLINK_TIME) {
      WIDGET->insp_blink=-PS_FIELD_INSERTION_POINT_BLINK_TIME;
    }
    if (WIDGET->insp_blink>=0) {
      int insx=textx-(WIDGET->size>>2)+((WIDGET->insp*WIDGET->size)>>1);
      struct akgl_vtx_raw vtxv[2]={
        {insx,texty-(WIDGET->size>>1),0x00,0x00,0x00,0xff},
        {insx,texty+(WIDGET->size>>1),0x00,0x00,0x00,0xff},
      };
      if (ps_video_draw_line_strip(vtxv,2)<0) return -1;
    }
  }

  if (ps_field_draw_border(widget,parentx+widget->x,parenty+widget->y)<0) return -1;
  
  return 0;
}

/* Measure.
 */

static int _ps_field_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=PS_FIELD_DEFAULT_WIDTH;
  *h=WIDGET->size+(PS_FIELD_MARGIN<<1);
  return 0;
}

/* Pack.
 */

static int _ps_field_pack(struct ps_widget *widget) {
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    // Set child bounds.
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_field_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  if (!value) return 0; // We only care about press and repeat.

  if ((codepoint>=0x20)&&(codepoint<=0x7e)) {
    if (ps_widget_field_append_char(widget,codepoint)<0) return -1;
    return 0;
  }

  switch (codepoint) {
    case 0x08: return ps_widget_field_backspace(widget);
    case 0x7f: return ps_widget_field_delete(widget);
  }

  switch (keycode) {
    case 0x00070050: return ps_widget_field_move_cursor(widget,-1);
    case 0x0007004f: return ps_widget_field_move_cursor(widget,1);
    case 0x0007004a: return ps_widget_field_move_cursor(widget,-2);
    case 0x0007004d: return ps_widget_field_move_cursor(widget,2);
  }

  return 0;
}

/* Digested input events.
 */

static int _ps_field_focus(struct ps_widget *widget) {
  WIDGET->focus=1;
  return 0;
}

static int _ps_field_unfocus(struct ps_widget *widget) {
  WIDGET->focus=0;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_field={

  .name="field",
  .objlen=sizeof(struct ps_widget_field),

  .del=_ps_field_del,
  .init=_ps_field_init,

  .draw=_ps_field_draw,
  .measure=_ps_field_measure,
  .pack=_ps_field_pack,

  .key=_ps_field_key,

  .focus=_ps_field_focus,
  .unfocus=_ps_field_unfocus,

};

/* Text accessors.
 */
 
int ps_widget_field_set_text(struct ps_widget *widget,const char *src,int srcc) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  WIDGET->text.c=0;
  if (ps_buffer_append(&WIDGET->text,src,srcc)<0) return -1;
  WIDGET->insp=WIDGET->text.c;
  return 0;
}

int ps_widget_field_get_text(void *dstpp,const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  if (dstpp) *(void**)dstpp=WIDGET->text.v;
  return WIDGET->text.c;
}

/* Insert one character.
 */
 
int ps_widget_field_append_char(struct ps_widget *widget,int codepoint) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  if ((codepoint<0x20)||(codepoint>0x7e)) return 0;
  char ch=codepoint;
  if (ps_buffer_replace(&WIDGET->text,WIDGET->insp,0,&ch,1)<0) return -1;
  WIDGET->insp++;
  return 0;
}

/* Delete one character.
 */
 
int ps_widget_field_backspace(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  if (WIDGET->insp<1) return 0;
  if (ps_buffer_replace(&WIDGET->text,WIDGET->insp-1,1,0,0)<0) return -1;
  WIDGET->insp--;
  return 0;
}

int ps_widget_field_delete(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  if (WIDGET->insp>=WIDGET->text.c) return 0;
  if (ps_buffer_replace(&WIDGET->text,WIDGET->insp,1,0,0)<0) return -1;
  return 0;
}

/* Move cursor.
 */
 
int ps_widget_field_move_cursor(struct ps_widget *widget,int d) {
  if (!widget||(widget->type!=&ps_widget_type_field)) return -1;
  if (d<-1) {
    WIDGET->insp=0;
  } else if (d==-1) {
    if (WIDGET->insp>0) WIDGET->insp--;
  } else if (d==1) {
    if (WIDGET->insp<WIDGET->text.c) WIDGET->insp++;
  } else if (d>1) {
    WIDGET->insp=WIDGET->text.c;
  }
  WIDGET->insp_blink=0; // Make insertion point visible.
  return 0;
}
