/* ps_widget_editsong.c
 * Outer level of song editor UI.
 * +----+-------------------+
 * |    | toolbar           |
 * |    +-------------------+
 * | 1  | chart             | 1:voicelist
 * |    |                   |
 * |    |                   |
 * +----+-------------------+
 */

#include "ps.h"
#include "gui/ps_gui.h"
#include "akau/akau.h"

#define PS_EDITSONG_VOICELIST_WIDTH   50
#define PS_EDITSONG_TOOLBAR_HEIGHT    12

static int ps_editsong_cb_play(struct ps_widget *label,void *userdata);
static int ps_editsong_cb_tempo(struct ps_widget *label,void *userdata);
static int ps_editsong_cb_transpose(struct ps_widget *label,void *userdata);
static int ps_editsong_cb_len(struct ps_widget *label,void *userdata);
static int ps_editsong_cb_add_drum(struct ps_widget *label,void *userdata);
static int ps_editsong_cb_add_instrument(struct ps_widget *label,void *userdata);

/* Widget definition.
 */

struct ps_widget_editsong {
  struct ps_widget hdr;
  struct akau_song *song;
  struct ps_widget *voicelist;
  struct ps_widget *chart;
  struct ps_widget *toolbar;
};

#define WIDGET ((struct ps_widget_editsong*)widget)

/* Delete.
 */

static void _ps_editsong_del(struct ps_widget *widget) {
  akau_song_del(WIDGET->song);
  ps_widget_del(WIDGET->voicelist);
  ps_widget_del(WIDGET->chart);
  ps_widget_del(WIDGET->toolbar);
}

/* Initialize.
 */

static int _ps_editsong_init(struct ps_widget *widget) {

  widget->bgrgba=0x008000ff;

  //TODO child widget types
  struct ps_widget *child;

  if (!(WIDGET->voicelist=ps_widget_spawn(widget,&ps_widget_type_songvoices))) return -1;
  if (ps_widget_ref(WIDGET->voicelist)<0) { WIDGET->voicelist=0; return -1; }

  if (!(WIDGET->chart=ps_widget_spawn(widget,&ps_widget_type_label))) return -1;
  if (ps_widget_ref(WIDGET->chart)<0) { WIDGET->chart=0; return -1; }
  if (ps_widget_label_set_text(WIDGET->chart,"chart",5)<0) return -1;
  WIDGET->chart->bgrgba=0x00ff00ff;

  if (!(WIDGET->toolbar=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1;
  if (ps_widget_ref(WIDGET->toolbar)<0) { WIDGET->toolbar=0; return -1; }
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"Play",4,ps_editsong_cb_play,widget,0))) return -1;
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"Tempo",5,ps_editsong_cb_tempo,widget,0))) return -1;
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"Xpose",5,ps_editsong_cb_transpose,widget,0))) return -1;
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"Len",3,ps_editsong_cb_len,widget,0))) return -1;
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"+D",2,ps_editsong_cb_add_drum,widget,0))) return -1;
  if (!(child=ps_widget_menubar_add_menu(WIDGET->toolbar,"+I",2,ps_editsong_cb_add_instrument,widget,0))) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_editsong_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_editsong_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_editsong_pack(struct ps_widget *widget) {
  if (widget->childc!=3) return -1;
  if (!WIDGET->voicelist) return -1;
  if (!WIDGET->toolbar) return -1;
  if (!WIDGET->chart) return -1;

  WIDGET->voicelist->x=0;
  WIDGET->voicelist->y=0;
  WIDGET->voicelist->w=PS_EDITSONG_VOICELIST_WIDTH;
  WIDGET->voicelist->h=widget->h;
  if (ps_widget_pack(WIDGET->voicelist)<0) return -1;

  WIDGET->toolbar->x=WIDGET->voicelist->w;
  WIDGET->toolbar->y=0;
  WIDGET->toolbar->w=widget->w-WIDGET->toolbar->x;
  WIDGET->toolbar->h=PS_EDITSONG_TOOLBAR_HEIGHT;
  if (ps_widget_pack(WIDGET->toolbar)<0) return -1;

  WIDGET->chart->x=WIDGET->voicelist->w;
  WIDGET->chart->y=WIDGET->toolbar->h;
  WIDGET->chart->w=widget->w-WIDGET->chart->x;
  WIDGET->chart->h=widget->h-WIDGET->chart->y;
  if (ps_widget_pack(WIDGET->chart)<0) return -1;
  
  return 0;
}

/* Mouse events.
 */

static int _ps_editsong_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsong_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsong_mousedown(struct ps_widget *widget,int btnid) {
  return 0;
}

static int _ps_editsong_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  return 0;
}

static int _ps_editsong_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_editsong_mousemove(struct ps_widget *widget,int x,int y) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsong={
  .name="editsong",
  .objlen=sizeof(struct ps_widget_editsong),
  .del=_ps_editsong_del,
  .init=_ps_editsong_init,
  .draw=_ps_editsong_draw,
  .measure=_ps_editsong_measure,
  .pack=_ps_editsong_pack,
  .mouseenter=_ps_editsong_mouseenter,
  .mouseexit=_ps_editsong_mouseexit,
  .mousedown=_ps_editsong_mousedown,
  .mouseup=_ps_editsong_mouseup,
  .mousewheel=_ps_editsong_mousewheel,
  .mousemove=_ps_editsong_mousemove,
};

/* Accessors.
 */
 
int ps_widget_editsong_set_song(struct ps_widget *widget,struct akau_song *song) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return -1;
  if (song&&(akau_song_ref(song)<0)) return -1;
  akau_song_del(WIDGET->song);
  WIDGET->song=song;
  return 0;
}

struct akau_song *ps_widget_editsong_get_song(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return 0;
  return WIDGET->song;
}

int ps_widget_editsong_set_path(struct ps_widget *widget,const char *path) {
  if (!widget||(widget->type!=&ps_widget_type_editsong)) return -1;
  //TODO
  return 0;
}

/* Playback.
 */

static int ps_editsong_playback_stop(struct ps_widget *widget) {
  if (akau_mixer_play_song(akau_get_mixer(),0,1)<0) return -1;
  return 0;
}

static int ps_editsong_playback_start(struct ps_widget *widget) {
  if (akau_mixer_play_song(akau_get_mixer(),WIDGET->song,1)<0) return -1;
  return 0;
}

/* Menu callbacks.
 */
 
static int ps_editsong_cb_play(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);
  if (akau_song_get_lock(WIDGET->song)) {
    if (ps_editsong_playback_stop(widget)<0) return -1;
    if (ps_widget_label_set_text(label,"Play",4)<0) return -1;
  } else {
    if (ps_editsong_playback_start(widget)<0) return -1;
    if (ps_widget_label_set_text(label,"Stop",4)<0) return -1;
  }
  if (ps_widget_pack(WIDGET->toolbar)<0) return -1;
  return 0;
}
 
static int ps_editsong_cb_tempo(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);

  struct ps_widget *modal=ps_widget_new(&ps_widget_type_dialogue);
  if (!modal) return -1;
  //modal->fgrgba=0xffffffff;
  //modal->bgrgba=0x000000ff;
  //ps_widget_label_set_text(modal,"Content of modal dialogue.",-1);
  if (
    (ps_widget_dialogue_set_message(modal,"Tempo:",-1)<0)||
    (ps_widget_dialogue_set_default(modal,"12345",5)<0)||
    //(ps_widget_dialogue_add_button(modal,"OK",2)<0)||
    //(ps_widget_dialogue_add_button(modal,"Cancel",6)<0)||
    (ps_page_push_modal(ps_widget_get_page(widget),modal)<0)
  ) {
    ps_widget_del(modal);
    return -1;
  }
  ps_widget_del(modal);
  
  return 0;
}
 
static int ps_editsong_cb_transpose(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);
  return 0;
}
 
static int ps_editsong_cb_len(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);
  return 0;
}
 
static int ps_editsong_cb_add_drum(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);
  return 0;
}
 
static int ps_editsong_cb_add_instrument(struct ps_widget *label,void *userdata) {
  struct ps_widget *widget=userdata;
  ps_log(EDIT,TRACE,"%s",__func__);
  return 0;
}
