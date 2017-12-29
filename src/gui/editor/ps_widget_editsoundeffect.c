/* ps_widget_editsoundeffect.c
 * Modal fullscreen controller for editing AKAU IPCM resources.
 *
 * Children:
 *   [0] menubar
 *   [1] scrolllist of sfxchan
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "../corewidgets/ps_corewidgets.h"
#include "akau/akau.h"
#include "resedit/ps_iwg.h"
#include "os/ps_fs.h"

static int ps_editsoundeffect_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsoundeffect_cb_play(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsoundeffect_cb_addchannel(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsoundeffect_cb_save(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_editsoundeffect {
  struct ps_widget hdr;
  struct akau_ipcm *ipcm;
  int resid;
  struct ps_iwg *iwg;
};

#define WIDGET ((struct ps_widget_editsoundeffect*)widget)

/* Delete.
 */

static void _ps_editsoundeffect_del(struct ps_widget *widget) {
  akau_ipcm_del(WIDGET->ipcm);
  ps_iwg_del(WIDGET->iwg);
}

/* Initialize.
 */

static int _ps_editsoundeffect_init(struct ps_widget *widget) {
  struct ps_widget *child;

  WIDGET->resid=-1;
  if (!(WIDGET->iwg=ps_iwg_new())) return -1;

  widget->bgrgba=0x000000ff;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1;
  { struct ps_widget *button;
    if (!(button=ps_widget_menubar_add_button(child,"Back",4,ps_callback(ps_editsoundeffect_cb_back,0,widget)))) return -1;
    if (!(button=ps_widget_menubar_add_button(child,"Play",4,ps_callback(ps_editsoundeffect_cb_play,0,widget)))) return -1;
    if (!(button=ps_widget_menubar_add_button(child,"+chan",5,ps_callback(ps_editsoundeffect_cb_addchannel,0,widget)))) return -1;
    if (!(button=ps_widget_menubar_add_button(child,"Save",4,ps_callback(ps_editsoundeffect_cb_save,0,widget)))) return -1;
  }

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1;
  
  return 0;
}

/* Pack.
 */

static int _ps_editsoundeffect_pack(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;
  struct ps_widget *menubar=widget->childv[0];
  struct ps_widget *scrolllist=widget->childv[1];
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;
  if (ps_widget_pack(menubar)<0) return -1;

  scrolllist->x=0;
  scrolllist->y=menubar->h;
  scrolllist->w=widget->w;
  scrolllist->h=widget->h-menubar->h;
  if (ps_widget_pack(scrolllist)<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsoundeffect={

  .name="editsoundeffect",
  .objlen=sizeof(struct ps_widget_editsoundeffect),

  .del=_ps_editsoundeffect_del,
  .init=_ps_editsoundeffect_init,

  .pack=_ps_editsoundeffect_pack,

};

/* Rebuild UI based on iwg.
 */

static int ps_editsoundeffect_rebuild_ui(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;
  if (!WIDGET->iwg) return -1;
  struct ps_widget *chanlist=widget->childv[1];
  if (ps_widget_remove_all_children(chanlist)<0) return -1;

  const struct ps_iwg_channel *chan=WIDGET->iwg->chanv;
  int i=0;
  for (;i<WIDGET->iwg->chanc;i++,chan++) {
    struct ps_widget *sfxchan=ps_widget_spawn(chanlist,&ps_widget_type_sfxchan);
    if (!sfxchan) return -1;
    if (ps_widget_sfxchan_setup(sfxchan,i)<0) return -1;
  }

  if (ps_widget_pack(chanlist)<0) return -1;

  return 0;
}

/* Locate source file and rebuild iwg from it.
 */

static int ps_editsoundeffect_rebuild_iwg_from_file(struct ps_widget *widget,int id) {

  struct akau_store *store=akau_get_store();
  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"ipcm",id);
  if ((pathc<0)||(pathc>=sizeof(path))) return -1;

  void *serial=0;
  int serialc=ps_file_read(&serial,path);
  if ((serialc<0)||!serial) return -1;

  if (ps_iwg_decode(WIDGET->iwg,serial,serialc)<0) {
    free(serial);
    return -1;
  }

  free(serial);
  return 0;
}

/* Compose default IWG.
 */

static int ps_editsoundeffect_compose_default_iwg(struct ps_widget *widget) {
  if (ps_iwg_clear(WIDGET->iwg)<0) return -1;
  if (ps_iwg_add_channel(WIDGET->iwg,"sine",4)<0) return -1;
  if (ps_iwg_add_command(WIDGET->iwg,   0,0,AKAU_WAVEGEN_K_TRIM,0.0)<0) return -1;
  if (ps_iwg_add_command(WIDGET->iwg,   0,0,AKAU_WAVEGEN_K_STEP,440.0)<0) return -1;
  if (ps_iwg_add_command(WIDGET->iwg,1000,0,AKAU_WAVEGEN_K_TRIM,0.0)<0) return -1;
  return 0;
}

/* Set resource.
 */
 
int ps_widget_editsoundeffect_set_resource(struct ps_widget *widget,int id,struct akau_ipcm *ipcm,const char *name) {
  if (!widget||(widget->type!=&ps_widget_type_editsoundeffect)||(widget->childc!=2)) return -1;
  
  if (akau_ipcm_ref(ipcm)<0) return -1;
  akau_ipcm_del(WIDGET->ipcm);
  WIDGET->resid=id;
  WIDGET->ipcm=ipcm;

  if (ps_widget_menubar_set_title(widget->childv[0],name,-1)<0) return -1;

  if (ps_editsoundeffect_rebuild_iwg_from_file(widget,id)<0) {
    ps_log(EDIT,ERROR,"Failed to rebuild ps_iwg in editsoundeffect for '%s' (ID %d).",name,id);
    if (ps_editsoundeffect_compose_default_iwg(widget)<0) return -1;
  }

  if (ps_editsoundeffect_rebuild_ui(widget)<0) return -1;
  
  return 0;
}

/* Menu callbacks.
 */
 
static int ps_editsoundeffect_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  return ps_widget_kill(widget);
}

static int ps_editsoundeffect_cb_play(struct ps_widget *button,struct ps_widget *widget) {
  struct akau_mixer *mixer=akau_get_mixer();

  /* Rebuild IPCM if necessary. */
  if (WIDGET->iwg->dirty) {
    struct akau_ipcm *nipcm=ps_iwg_to_ipcm(WIDGET->iwg);
    if (!nipcm) return -1;
    akau_ipcm_del(WIDGET->ipcm);
    WIDGET->ipcm=nipcm;
    WIDGET->iwg->dirty=0;
  }
  
  if (akau_mixer_play_ipcm(mixer,WIDGET->ipcm,0xff,0,0)<0) return -1;
  return 0;
}

static int ps_editsoundeffect_cb_addchannel(struct ps_widget *button,struct ps_widget *widget) {
  int chanid=ps_iwg_add_channel(WIDGET->iwg,"sine",4);
  if (chanid<0) return -1;
  WIDGET->iwg->dirty=1;
  if (ps_editsoundeffect_rebuild_ui(widget)<0) return -1;
  return 0;
}

static int ps_editsoundeffect_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  struct akau_store *store=akau_get_store();
  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"ipcm",WIDGET->resid);
  if ((pathc<0)||(pathc>=sizeof(path))) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"ipcm",WIDGET->resid);
    if ((pathc<0)||(pathc>=sizeof(path))) return -1;
  }

  void *serial=0;
  int serialc=ps_iwg_encode(&serial,WIDGET->iwg);
  if ((serialc<0)||!serial) return -1;

  if (ps_file_write(path,serial,serialc)<0) {
    free(serial);
    return -1;
  }
  free(serial);

  struct akau_ipcm *nipcm=ps_iwg_to_ipcm(WIDGET->iwg);
  if (!nipcm) return -1;
  akau_ipcm_del(WIDGET->ipcm);
  WIDGET->ipcm=nipcm;
  if (akau_store_replace_ipcm(store,WIDGET->ipcm,WIDGET->resid)<0) return -1;
  if (akau_store_relink_songs(store)<0) return -1;

  WIDGET->iwg->dirty=0;
  ps_log(EDIT,DEBUG,"Saved %d bytes to %s",serialc,path);
  return 0;
}

/* Get iwg.
 */
 
struct ps_iwg *ps_widget_editsoundeffect_get_iwg(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsoundeffect)) return 0;
  return WIDGET->iwg;
}

/* Rebuild all descendant graphs.
 */
 
int ps_widget_editsoundeffect_rebuild_graphs(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsoundeffect)||(widget->childc!=2)) return -1;
  struct ps_widget *chanlist=widget->childv[1];
  int i=chanlist->childc; while (i-->0) {
    struct ps_widget *sfxchan=chanlist->childv[i];
    if (ps_widget_sfxchan_rebuild_graphs(sfxchan)<0) return -1;
  }
  return 0;
}

/* Reassign channel ID to all descendant channel widgets.
 * You must call this after removing a channel.
 */

int ps_widget_editsoundeffect_reassign_channel_ids(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsoundeffect)||(widget->childc!=2)) return -1;
  struct ps_widget *chanlist=widget->childv[1];
  int i=0; for (;i<chanlist->childc;i++) {
    struct ps_widget *sfxchan=chanlist->childv[i];
    if (ps_widget_sfxchan_set_chanid(sfxchan,i)<0) return -1;
  }
  return 0;
}
