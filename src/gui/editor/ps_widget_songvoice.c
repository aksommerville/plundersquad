/* ps_widget_songvoice.c
 * Controls for one voice (drum or instrument) in a song.
 * 
 * Children:
 *  [0] Edit button (IPCM ID or wave editor)
 *  [1] Delete button
 *  [2] Display toggle
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"

#define PS_SONGVOICE_BGCOLOR_DRUM_EVEN     0x4080a0ff
#define PS_SONGVOICE_BGCOLOR_DRUM_ODD      0x60a0c0ff
#define PS_SONGVOICE_BGCOLOR_INSTR_EVEN    0x40a080ff
#define PS_SONGVOICE_BGCOLOR_INSTR_ODD     0x60c0a0ff

#define PS_SONGVOICE_PREFERRED_WIDTH   60
#define PS_SONGVOICE_PREFERRED_HEIGHT  30
#define PS_SONGVOICE_SPACING            2

static int ps_songvoice_cb_edit(struct ps_widget *button,struct ps_widget *widget);
static int ps_songvoice_cb_delete(struct ps_widget *button,struct ps_widget *widget);
static int ps_songvoice_cb_displaytoggle(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_songvoice {
  struct ps_widget hdr;
  int drumid,instrid; // One is <0, the other is what we're in control of.
};

#define WIDGET ((struct ps_widget_songvoice*)widget)

/* Delete.
 */

static void _ps_songvoice_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_songvoice_init(struct ps_widget *widget) {

  WIDGET->drumid=-1;
  WIDGET->instrid=-1;

  struct ps_widget *child;

  if (!(child=ps_widget_button_spawn(widget,0,"EDIT",4,ps_callback(ps_songvoice_cb_edit,0,widget)))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"X",1,ps_callback(ps_songvoice_cb_delete,0,widget)))) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_toggle))) return -1;
    if (!ps_widget_toggle_set_text(child,"V",1)) return -1;
    if (ps_widget_toggle_set_callback(child,ps_callback(ps_songvoice_cb_displaytoggle,0,widget))<0) return -1;
    if (ps_widget_toggle_set_value(child,1)<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_songvoice_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_songvoice) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_songvoice_get_editbutton(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_songvoice_get_deletebutton(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_songvoice_get_displaytoggle(const struct ps_widget *widget) {
  return widget->childv[2];
}

/* Draw.
 */

static int _ps_songvoice_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_songvoice_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  if (ps_songvoice_obj_validate(widget)<0) return -1;
  *w=PS_SONGVOICE_PREFERRED_WIDTH;
  *h=PS_SONGVOICE_PREFERRED_HEIGHT;
  return 0;
}

/* Pack.
 * +----------------+-----+
 * |                |  B  |
 * |         A      +-----+
 * |                |  C  |
 * +----------------+-----+
 * Each line has a width of PS_SONGVOICE_SPACING.
 * (B,C) are both square, so their dimensions are known from parent height.
 * (A) takes whatever is left.
 * A=editbutton
 * B=deletebutton
 * c=displaytoggle
 */

static int _ps_songvoice_pack(struct ps_widget *widget) {
  if (ps_songvoice_obj_validate(widget)<0) return -1;
  struct ps_widget *editbutton=ps_songvoice_get_editbutton(widget);
  struct ps_widget *deletebutton=ps_songvoice_get_deletebutton(widget);
  struct ps_widget *displaytoggle=ps_songvoice_get_displaytoggle(widget);

  /* If we can't give at least one pixel to each child, panic. */
 _panic_:;
  const int minw=(PS_SONGVOICE_SPACING*3)+2;
  const int minh=(PS_SONGVOICE_SPACING*3)+2;
  if ((widget->w<minw)||(widget->h<minh)) {
    int i=widget->childc; while (i-->0) {
      struct ps_widget *child=widget->childv[i];
      child->x=child->y=0;
      child->w=widget->w;
      child->h=widget->h;
    }
    goto _pack_;
  }

  /* Establish sizes of (B,C). Panic if they end up too wide. */
  int availw=widget->w-(PS_SONGVOICE_SPACING*3);
  int availh=widget->h-(PS_SONGVOICE_SPACING*3);
  deletebutton->h=availh>>1;
  displaytoggle->h=availh-deletebutton->h;
  deletebutton->w=deletebutton->h;
  displaytoggle->w=deletebutton->w;
  if (deletebutton->w>=availw) goto _panic_;

  /* Size of (A) is pretty simple now. */
  editbutton->w=availw-deletebutton->w;
  editbutton->h=widget->h-(PS_SONGVOICE_SPACING*2);

  /* Now that the sizes are known, positions are a walk in the park. */
  editbutton->x=PS_SONGVOICE_SPACING;
  editbutton->y=PS_SONGVOICE_SPACING;
  deletebutton->x=editbutton->x+editbutton->w+PS_SONGVOICE_SPACING;
  deletebutton->y=PS_SONGVOICE_SPACING;
  displaytoggle->x=deletebutton->x;
  displaytoggle->y=deletebutton->y+deletebutton->h+PS_SONGVOICE_SPACING;

 _pack_:;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_songvoice={

  .name="songvoice",
  .objlen=sizeof(struct ps_widget_songvoice),

  .del=_ps_songvoice_del,
  .init=_ps_songvoice_init,

  .draw=_ps_songvoice_draw,
  .measure=_ps_songvoice_measure,
  .pack=_ps_songvoice_pack,

};

/* Set label of editbutton to whatever is appropriate.
 */

static int ps_songvoice_reset_editbutton_label(struct ps_widget *widget) {
  struct ps_widget *editbutton=ps_songvoice_get_editbutton(widget);
  struct akau_song *song=ps_widget_editsong_get_song(widget);
  
  if (WIDGET->drumid>=0) {
    int ipcmid=akau_song_get_drum_ipcmid(song,WIDGET->drumid);
    if (ipcmid<0) goto _fault_;
    char buf[32];
    int bufc=snprintf(buf,sizeof(buf),"SFX %d",ipcmid);
    if ((bufc<1)||(bufc>=sizeof(buf))) goto _fault_;
    if (ps_widget_button_set_text(editbutton,buf,bufc)<0) return -1;
    
  } else if (WIDGET->instrid>=0) {
    char buf[32];
    int bufc=snprintf(buf,sizeof(buf),"I %d",WIDGET->instrid);
    if ((bufc<1)||(bufc>=sizeof(buf))) goto _fault_;
    if (ps_widget_button_set_text(editbutton,buf,bufc)<0) return -1;
  
  } else {
   _fault_:
    if (ps_widget_button_set_text(editbutton,"?",1)<0) return -1;
  }
  return 0;
}

/* Setup.
 */
 
int ps_widget_songvoice_setup_drum(struct ps_widget *widget,int drumid) {
  if (ps_songvoice_obj_validate(widget)<0) return -1;
  if (WIDGET->drumid>=0) return -1;
  if (WIDGET->instrid>=0) return -1;
  if (drumid<0) return -1;
  struct akau_song *song=ps_widget_editsong_get_song(widget);
  if (!song) return -1;
  if (drumid>=akau_song_count_drums(song)) return -1;
  WIDGET->drumid=drumid;
  if (drumid&1) widget->bgrgba=PS_SONGVOICE_BGCOLOR_DRUM_ODD;
  else widget->bgrgba=PS_SONGVOICE_BGCOLOR_DRUM_EVEN;
  if (ps_songvoice_reset_editbutton_label(widget)<0) return -1;
  return 0;
}

int ps_widget_songvoice_setup_instrument(struct ps_widget *widget,int instrid) {
  if (ps_songvoice_obj_validate(widget)<0) return -1;
  if (WIDGET->drumid>=0) return -1;
  if (WIDGET->instrid>=0) return -1;
  if (instrid<0) return -1;
  struct akau_song *song=ps_widget_editsong_get_song(widget);
  if (!song) return -1;
  if (instrid>=akau_song_count_instruments(song)) return -1;
  WIDGET->instrid=instrid;
  if (instrid&1) widget->bgrgba=PS_SONGVOICE_BGCOLOR_INSTR_ODD;
  else widget->bgrgba=PS_SONGVOICE_BGCOLOR_INSTR_EVEN;
  if (ps_songvoice_reset_editbutton_label(widget)<0) return -1;
  return 0;
}

/* Button callbacks.
 */
 
static int ps_songvoice_cb_edit(struct ps_widget *button,struct ps_widget *widget) {
  if (WIDGET->drumid>=0) {
    if (ps_widget_editsong_edit_drum(widget,WIDGET->drumid)<0) return -1;
  } else if (WIDGET->instrid>=0) {
    if (ps_widget_editsong_edit_instrument(widget,WIDGET->instrid)<0) return -1;
  }
  return 0;
}

static int ps_songvoice_cb_delete(struct ps_widget *button,struct ps_widget *widget) {
  if (WIDGET->drumid>=0) {
    if (ps_widget_editsong_delete_drum(widget,WIDGET->drumid,0)<0) return -1;
  } else if (WIDGET->instrid>=0) {
    if (ps_widget_editsong_delete_instrument(widget,WIDGET->instrid,0)<0) return -1;
  }
  return 0;
}

static int ps_songvoice_cb_displaytoggle(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_editsong_voice_visibility_changed(widget)<0) return -1;
  return 0;
}

/* Accessors.
 */
 
int ps_widget_songvoice_get_type(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_songvoice)) return 0;
  if (WIDGET->drumid>=0) return 1;
  if (WIDGET->instrid>=0) return 2;
  return 0;
}

uint8_t ps_widget_songvoice_get_id(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_songvoice)) return 0;
  if (WIDGET->drumid>=0) return WIDGET->drumid;
  if (WIDGET->instrid>=0) return WIDGET->instrid;
  return 0;
}

int ps_widget_songvoice_get_visibility(const struct ps_widget *widget) {
  if (ps_songvoice_obj_validate(widget)<0) return 0;
  struct ps_widget *displaytoggle=ps_songvoice_get_displaytoggle(widget);
  return ps_widget_toggle_get_value(displaytoggle);
}
