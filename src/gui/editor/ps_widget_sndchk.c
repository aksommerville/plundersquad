/* ps_widget_sndchk.c
 * Interactive sound check.
 * We play a song constantly and allow the user to change it.
 * User can press his button to play a sound effect.
 * We display in real time the actual output levels.
 *
 * Children:
 *   [0] songlbl
 *   [1] ipcmlbl
 *   [2] levelgraphl
 *   [3] levelgraphr
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/editor/ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "akau/akau.h"

static int ps_sndchk_change_song(struct ps_widget *widget,int d);
static int ps_sndchk_change_ipcm(struct ps_widget *widget,int d);
static int ps_sndchk_play(struct ps_widget *widget);
static int ps_sndchk_dismiss(struct ps_widget *widget);
static void ps_sndchk_cb_audio(const int16_t *v,int c,void *userdata);

/* Object definition.
 */

struct ps_widget_sndchk {
  struct ps_widget hdr;
  int songp; // Position in resource store or -1
  int ipcmp; // Position in resource store or -1
};

#define WIDGET ((struct ps_widget_sndchk*)widget)

/* Delete.
 */

static void _ps_sndchk_del(struct ps_widget *widget) {
  akau_play_song(0,1);
  akau_set_observer(0,0);
  akau_mixer_set_print_songs(akau_get_mixer(),0);
}

/* Initialize.
 */

static int _ps_sndchk_init(struct ps_widget *widget) {

  WIDGET->songp=-1;
  WIDGET->ipcmp=-1;

  widget->bgrgba=0xc0a000ff;

  struct ps_widget *child;
  if (!(child=ps_widget_label_spawn(widget,"song: (none)",-1))) return -1;
  if (!(child=ps_widget_label_spawn(widget,"ipcm: (none)",-1))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_levelgraph))) return -1;
  child->bgrgba=0xd0d0d0ff;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_levelgraph))) return -1;
  child->bgrgba=0xff7090ff;

  if (akau_set_observer(ps_sndchk_cb_audio,widget)<0) return -1;
  if (akau_mixer_set_print_songs(akau_get_mixer(),1)<0) return -1;
  
  return 0;
}

/* Children.
 */

static int ps_sndchk_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_sndchk) return -1;
  if (widget->childc!=4) return -1;
  return 0;
}

static struct ps_widget *ps_sndchk_get_songlbl(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_sndchk_get_ipcmlbl(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_sndchk_get_levelgraphl(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_sndchk_get_levelgraphr(const struct ps_widget *widget) { return widget->childv[3]; }

/* Pack.
 */

static int _ps_sndchk_pack(struct ps_widget *widget) {
  if (ps_sndchk_obj_validate(widget)<0) return -1;
  struct ps_widget *songlbl=ps_sndchk_get_songlbl(widget);
  struct ps_widget *ipcmlbl=ps_sndchk_get_ipcmlbl(widget);
  struct ps_widget *levelgraphl=ps_sndchk_get_levelgraphl(widget);
  struct ps_widget *levelgraphr=ps_sndchk_get_levelgraphr(widget);

  int chw,chh;

  /* songlbl and ipcmlbl go at the top, each taking half of the total width. */
  if (ps_widget_measure(&chw,&chh,songlbl,widget->w,widget->h)<0) return -1;
  songlbl->x=0;
  songlbl->y=0;
  songlbl->w=widget->w>>1;
  songlbl->h=chh;
  ipcmlbl->x=songlbl->w;
  ipcmlbl->y=0;
  ipcmlbl->w=widget->w-ipcmlbl->x;
  ipcmlbl->h=songlbl->h;

  /* levelgraphl and levelgraphr each take the full width and divide the remaining height evenly. */
  levelgraphl->x=0;
  levelgraphl->y=songlbl->h;
  levelgraphl->w=widget->w;
  levelgraphl->h=(widget->h-levelgraphl->y)>>1;
  levelgraphr->x=0;
  levelgraphr->y=levelgraphl->y+levelgraphl->h;
  levelgraphr->w=widget->w;
  levelgraphr->h=widget->h-levelgraphr->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_sndchk_update(struct ps_widget *widget) {
  if (ps_sndchk_obj_validate(widget)<0) return -1;
  struct ps_widget *levelgraphl=ps_sndchk_get_levelgraphl(widget);
  struct ps_widget *levelgraphr=ps_sndchk_get_levelgraphr(widget);
  if (ps_widget_update(levelgraphl)<0) return -1;
  if (ps_widget_update(levelgraphr)<0) return -1;
  return 0;
}

/* Primitive input events.
 */

static int _ps_sndchk_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (!value) return 0;
  switch (btnid) {
    case PS_PLRBTN_UP: return ps_sndchk_change_ipcm(widget,-1);
    case PS_PLRBTN_DOWN: return ps_sndchk_change_ipcm(widget,1);
    case PS_PLRBTN_LEFT: return ps_sndchk_change_song(widget,-1);
    case PS_PLRBTN_RIGHT: return ps_sndchk_change_song(widget,1);
    case PS_PLRBTN_A: return ps_sndchk_play(widget);
    case PS_PLRBTN_B: return ps_sndchk_dismiss(widget);
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_sndchk={

  .name="sndchk",
  .objlen=sizeof(struct ps_widget_sndchk),

  .del=_ps_sndchk_del,
  .init=_ps_sndchk_init,

  .pack=_ps_sndchk_pack,

  .update=_ps_sndchk_update,
  .userinput=_ps_sndchk_userinput,

};

/* Remove dir and extension from path, in place.
 */

static int ps_sndchk_trim_resource_name(char *v,int c) {
  int i=c;
  while (i-->0) {
    if (v[i]=='.') c=i;
    else if ((v[i]=='/')||(v[i]=='\\')) {
      int np=i+1;
      int nc=c-np;
      memmove(v,v+np,nc);
      return nc;
    }
  }
  return c;
}

/* Change song.
 */
 
static int ps_sndchk_change_song(struct ps_widget *widget,int d) {

  const struct ps_restype *restype=PS_RESTYPE(SONG);
  if (!restype) return -1;
  if (restype->resc<1) return 0;
  if (WIDGET->songp<0) {
    if (d<0) WIDGET->songp=restype->resc-1;
    else WIDGET->songp=0;
  } else {
    WIDGET->songp+=d;
    if (WIDGET->songp<0) WIDGET->songp=-1;
    else if (WIDGET->songp>=restype->resc) WIDGET->songp=-1;
  }

  int songid=0;
  struct ps_widget *songlbl=ps_sndchk_get_songlbl(widget);
  if (WIDGET->songp<0) {
    if (ps_widget_label_set_text(songlbl,"song: (none)",-1)<0) return -1;
  } else {
    const struct ps_res *res=restype->resv+WIDGET->songp;
    songid=res->id;
    if (ps_widget_label_set_textf(songlbl,"song: %d",res->id)<0) return -1;
  }

  if (akau_play_song(songid,1)<0) return -1;
  
  return 0;
}

/* Change sound effect.
 */
 
static int ps_sndchk_change_ipcm(struct ps_widget *widget,int d) {

  const struct ps_restype *restype=PS_RESTYPE(IPCM);
  if (!restype) return -1;
  if (restype->resc<1) return 0;
  if (WIDGET->ipcmp<0) {
    if (d<0) WIDGET->ipcmp=restype->resc-1;
    else WIDGET->ipcmp=0;
  } else {
    WIDGET->ipcmp+=d;
    if (WIDGET->ipcmp<0) WIDGET->ipcmp=-1;
    else if (WIDGET->ipcmp>=restype->resc) WIDGET->ipcmp=-1;
  }
  
  struct ps_widget *ipcmlbl=ps_sndchk_get_ipcmlbl(widget);
  if (WIDGET->ipcmp<0) {
    if (ps_widget_label_set_text(ipcmlbl,"ipcm: (none)",-1)<0) return -1;
  } else {
    const struct ps_res *res=restype->resv+WIDGET->ipcmp;
    char buf[256];
    int bufc=ps_res_get_path_for_resource(buf,sizeof(buf),PS_RESTYPE_IPCM,res->id,1);
    bufc=ps_sndchk_trim_resource_name(buf,bufc);
    if ((bufc<1)||(bufc>=sizeof(buf))) {
      if (ps_widget_label_set_textf(ipcmlbl,"ipcm: %d",res->id)<0) return -1;
    } else {
      if (ps_widget_label_set_text(ipcmlbl,buf,bufc)<0) return -1;
    }
  }
  
  return 0;
}

/* Play sound effect.
 */
 
static int ps_sndchk_play(struct ps_widget *widget) {
  if (WIDGET->ipcmp<0) return 0;
  const struct ps_restype *restype=PS_RESTYPE(IPCM);
  if (!restype) return -1;
  if (WIDGET->ipcmp>=restype->resc) return -1;
  int resid=restype->resv[WIDGET->ipcmp].id;
  if (akau_play_sound(resid,0x80,0)<0) return -1;
  return 0;
}

/* Dismiss.
 */
 
static int ps_sndchk_dismiss(struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Callback from AKAU.
 */
 
static void ps_sndchk_cb_audio(const int16_t *v,int c,void *userdata) {
  c&=~1;
  if (!v||(c<1)) return;
  struct ps_widget *widget=userdata;
  if (ps_sndchk_obj_validate(widget)<0) return;
  struct ps_widget *levelgraphl=ps_sndchk_get_levelgraphl(widget);
  struct ps_widget *levelgraphr=ps_sndchk_get_levelgraphr(widget);
  for (;(c-=2)>0;v+=2) {
    if (ps_widget_levelgraph_add_sample(levelgraphl,v[0])<0) return;
    if (ps_widget_levelgraph_add_sample(levelgraphr,v[1])<0) return;
  }
}
