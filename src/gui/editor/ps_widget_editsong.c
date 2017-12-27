/* ps_widget_editsong.c
 *
 * Children:
 *  [0] menubar
 *  [1] voice list (drums and instruments)
 *  [2] songchart
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "os/ps_fs.h"
#include "akau/akau.h"
#include "resedit/ps_sem.h"

#define PS_EDITSONG_VOICELIST_WIDTH_MIN 40
#define PS_EDITSONG_BEAT_LIMIT 0x01000000 /* Just a sanity limit, no particular reason for it. */

static int ps_editsong_cb_back(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_cb_save(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_cb_tempo(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_cb_add_drum(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_cb_add_instrument(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_cb_len(struct ps_widget *button,struct ps_widget *widget);
static int ps_editsong_stop(struct ps_widget *widget);
static int ps_editsong_cb_mixer_sync(struct akau_song *song,int beatp,void *userdata);

/* Object definition.
 */

struct ps_widget_editsong {
  struct ps_widget hdr;
  int resid;

  /* (song) is the authority for tempo and voice list.
   * (sem) is the authority for commands.
   * Initially, we decode into (song) and copy from there to (sem).
   * We rebuild (song) from (sem) as needed.
   */
  struct akau_song *song;
  struct ps_sem *sem;
  
  int dirty;
  int beatc;
  int playing;
  int play_beatp;
};

#define WIDGET ((struct ps_widget_editsong*)widget)

/* Delete.
 */

static void _ps_editsong_del(struct ps_widget *widget) {

  struct akau_mixer *mixer=akau_get_mixer();
  akau_mixer_play_song(mixer,0,0);
  akau_song_set_sync_callback(WIDGET->song,0,0);
  
  akau_song_del(WIDGET->song);
  ps_sem_del(WIDGET->sem);
}

/* Initialize.
 */

static int _ps_editsong_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0x00ffffff;
  widget->accept_keyboard_focus=1;

  WIDGET->play_beatp=-1;

  if (!(WIDGET->sem=ps_sem_new())) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_menubar))) return -1; // menubar
  if (!ps_widget_menubar_add_button(child,"Back",4,ps_callback(ps_editsong_cb_back,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Save",4,ps_callback(ps_editsong_cb_save,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Tempo",5,ps_callback(ps_editsong_cb_tempo,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"+D",2,ps_callback(ps_editsong_cb_add_drum,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"+I",2,ps_callback(ps_editsong_cb_add_instrument,0,widget))) return -1;
  if (!ps_widget_menubar_add_button(child,"Len",3,ps_callback(ps_editsong_cb_len,0,widget))) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1; // voicelist
  child->bgrgba=0x000000ff;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_songchart))) return -1; // songchart

  return 0;
}

/* Structural validation and access.
 */

static int ps_editsong_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_editsong) return -1;
  if (widget->childc!=3) return -1;
  return 0;
}

static struct ps_widget *ps_editsong_get_menubar(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_editsong_get_voicelist(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_editsong_get_songchart(const struct ps_widget *widget) {
  return widget->childv[2];
}

/* Pack.
 */

static int _ps_editsong_pack(struct ps_widget *widget) {
  if (ps_editsong_obj_validate(widget)<0) return -1;
  struct ps_widget *menubar=ps_editsong_get_menubar(widget);
  struct ps_widget *voicelist=ps_editsong_get_voicelist(widget);
  struct ps_widget *songchart=ps_editsong_get_songchart(widget);
  int chw,chh;

  /* Menubar on top with full width and whatever height it wants. */
  if (ps_widget_measure(&chw,&chh,menubar,widget->w,widget->h)<0) return -1;
  menubar->x=0;
  menubar->y=0;
  menubar->w=widget->w;
  menubar->h=chh;

  /* Voicelist on left with its preferred width or a minimum, and the remaining height. */
  if (ps_widget_measure(&chw,&chh,voicelist,widget->w,widget->h-menubar->h)<0) return -1;
  voicelist->x=0;
  voicelist->y=menubar->h;
  voicelist->w=(chw>PS_EDITSONG_VOICELIST_WIDTH_MIN)?chw:PS_EDITSONG_VOICELIST_WIDTH_MIN;
  voicelist->h=widget->h-menubar->h;

  /* Songchart gets everything remaining. */
  songchart->x=voicelist->w;
  songchart->y=menubar->h;
  songchart->w=widget->w-songchart->x;
  songchart->h=widget->h-songchart->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_editsong_update(struct ps_widget *widget) {
  return 0;
}

/* Primitive input events.
 */

static int _ps_editsong_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  if (value) switch (codepoint) {
    case ' ': return ps_widget_editsong_toggle_playback(widget);
  }
  return ps_widget_key(ps_editsong_get_songchart(widget),keycode,codepoint,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsong={

  .name="editsong",
  .objlen=sizeof(struct ps_widget_editsong),

  .del=_ps_editsong_del,
  .init=_ps_editsong_init,

  .pack=_ps_editsong_pack,

  .update=_ps_editsong_update,
  .key=_ps_editsong_key,

};

/* Rebuild voice list.
 */

static int ps_editsong_rebuild_voicelist(struct ps_widget *widget) {
  struct ps_widget *voicelist=ps_editsong_get_voicelist(widget);
  if (ps_widget_remove_all_children(voicelist)<0) return -1;
  if (!WIDGET->song) return 0;
  int i;

  int drumc=akau_song_count_drums(WIDGET->song);
  for (i=0;i<drumc;i++) {
    struct ps_widget *songvoice=ps_widget_spawn(voicelist,&ps_widget_type_songvoice);
    if (!songvoice) return -1;
    if (ps_widget_songvoice_setup_drum(songvoice,i)<0) return -1;
  }

  int instrc=akau_song_count_instruments(WIDGET->song);
  for (i=0;i<instrc;i++) {
    struct ps_widget *songvoice=ps_widget_spawn(voicelist,&ps_widget_type_songvoice);
    if (!songvoice) return -1;
    if (ps_widget_songvoice_setup_instrument(songvoice,i)<0) return -1;
  }

  return 0;
}

/* Rebuild song chart.
 */

static int ps_editsong_rebuild_songchart(struct ps_widget *widget) {
  struct ps_widget *songchart=ps_editsong_get_songchart(widget);
  if (ps_widget_songchart_rebuild(songchart)<0) return -1;
  return 0;
}

/* Set resource.
 */
 
int ps_widget_editsong_set_resource(struct ps_widget *widget,int id,struct akau_song *song,const char *name) {
  if (ps_editsong_obj_validate(widget)<0) return -1;
  
  WIDGET->resid=id;

  ps_editsong_stop(widget);

  if (akau_song_ref(song)<0) return -1;
  akau_song_del(WIDGET->song);
  WIDGET->song=song;
  if ((WIDGET->beatc=akau_song_count_beats(WIDGET->song))<0) return -1;

  if (ps_sem_read_song(WIDGET->sem,WIDGET->song)<0) return -1;

  //ps_sem_dump(WIDGET->sem);

  if (ps_widget_menubar_set_title(ps_editsong_get_menubar(widget),name,-1)<0) return -1;

  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_editsong_rebuild_songchart(widget)<0) return -1;

  return 0;
}

/* Toggle playback.
 */
 
int ps_widget_editsong_toggle_playback(struct ps_widget *widget) {
  if (ps_editsong_obj_validate(widget)<0) return -1;
  struct akau_mixer *mixer=akau_get_mixer();
  if (!mixer) return -1;
  if (WIDGET->playing) {
    if (akau_mixer_play_song(mixer,0,1)<0) return -1;
    WIDGET->playing=0;
    WIDGET->play_beatp=-1;
  } else {

    WIDGET->play_beatp=0;
    if (ps_sem_write_song(WIDGET->song,WIDGET->sem)<0) return -1;
    if (akau_song_set_sync_callback(WIDGET->song,ps_editsong_cb_mixer_sync,widget)<0) return -1;
    if (akau_mixer_play_song(mixer,0,1)<0) return -1;

    if (akau_song_link(WIDGET->song,akau_get_store())<0) {
      ps_log(EDIT,ERROR,"Failed to link song.");
      WIDGET->play_beatp=-1;
      return 0;
    }
    
    if (akau_mixer_play_song(mixer,WIDGET->song,1)<0) {
      ps_log(EDIT,ERROR,"Failed to begin song playback.");
      WIDGET->play_beatp=-1;
    } else {
      WIDGET->playing=1;
    }
  }
  return 0;
}

static int ps_editsong_stop(struct ps_widget *widget) {
  struct akau_mixer *mixer=akau_get_mixer();
  if (akau_mixer_play_song(mixer,0,1)<0) return -1;
  if (akau_mixer_stop_all(mixer,100)<0) return -1;
  WIDGET->playing=0;
  WIDGET->play_beatp=-1;
  return 0;
}

/* Mixer callback.
 */
 
static int ps_editsong_cb_mixer_sync(struct akau_song *song,int beatp,void *userdata) {
  struct ps_widget *widget=userdata;
  WIDGET->play_beatp=beatp;
  return 0;
}

/* Get playing beat.
 */
 
int ps_widget_editsong_get_current_beat(const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (!widget) return -1;
  return WIDGET->play_beatp;
}

/* Button callbacks.
 */
 
static int ps_editsong_cb_back(struct ps_widget *button,struct ps_widget *widget) {
  ps_editsong_stop(widget);
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}
 
static int ps_editsong_cb_save(struct ps_widget *button,struct ps_widget *widget) {

  struct akau_store *store=akau_get_store();
  char path[1024];
  int pathc=akau_store_get_resource_path(path,sizeof(path),store,"song",WIDGET->resid);
  if (pathc<0) {
    pathc=akau_store_generate_resource_path(path,sizeof(path),store,"song",WIDGET->resid);
    if ((pathc<1)||(pathc>=sizeof(path))) {
      ps_log(EDIT,ERROR,"Failed to compose path for song:%d",WIDGET->resid);
      return 0;
    }
  }

  ps_editsong_stop(widget);

  if (ps_sem_write_song(WIDGET->song,WIDGET->sem)<0) {
    ps_log(EDIT,ERROR,"Failed to rebuild akau_song from ps_sem [ps_sem_write_song]");
    return 0;
  }

  void *serial=0;
  int serialc=akau_song_encode(&serial,WIDGET->song);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to serialize song.");
    return 0;
  }

  if (ps_file_write(path,serial,serialc)<0) {
    ps_log(EDIT,ERROR,"Failed to write song:%d to '%s'.",WIDGET->resid,path);
    free(serial);
    return 0;
  }

  ps_log(EDIT,INFO,"Wrote song:%d to '%s', %d bytes.",WIDGET->resid,path,serialc);
  free(serial);
  
  return 0;
}

static int ps_editsong_cb_tempo_changed(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  if (ps_editsong_obj_validate(widget)<0) return -1;
  int tempo;
  if (ps_widget_dialogue_get_number(&tempo,dialogue)<0) return -1;
  ps_editsong_stop(widget);
  if (akau_song_set_tempo(WIDGET->song,tempo)<0) return -1;
  WIDGET->dirty=1;
  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editsong_cb_tempo(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Tempo",5,
    akau_song_get_tempo(WIDGET->song),
    ps_editsong_cb_tempo_changed
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  
  return 0;
}

static int ps_editsong_cb_add_drum(struct ps_widget *button,struct ps_widget *widget) {

  /* It is required to have a valid IPCM ID when creating a drum.
   * Use the first in the store.
   * If there are no IPCMs, you can't add drums to a song.
   */
  struct akau_store *store=akau_get_store();
  int ipcmid=akau_store_get_ipcm_id_by_index(store,0);
  if (ipcmid<0) {
    ps_log(EDIT,ERROR,"AKAU store contains no IPCMs, or failed to report them. Can't add drum to song.");
    return 0;
  }

  ps_editsong_stop(widget);
  int drumid=akau_song_add_drum(WIDGET->song,ipcmid);
  if (drumid<0) return -1;
  WIDGET->dirty=1;
  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  return 0;
}

static int ps_editsong_cb_add_instrument(struct ps_widget *button,struct ps_widget *widget) {

  /* Sensible default instrument: sine wave with fast response.
   */
  const uint8_t minstr[]={
    0,10, // attack time
    0,10, // drawback time
    0,50, // decay time
    0x80, // attack trim
    0x60, // drawback trim
    0, // unused
    1, // coefficient count
    0xff,0xff, // coefficients...
  };

  ps_editsong_stop(widget);
  int instrid=akau_song_add_instrument(WIDGET->song,minstr,sizeof(minstr));
  if (instrid<0) return -1;
  WIDGET->dirty=1;

  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

/* Edit song length.
 */
 
static int ps_editsong_cb_len_ok(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int nbeatc=0;
  if (ps_widget_dialogue_get_number(&nbeatc,dialogue)<0) return 0; // Try again.
  if ((nbeatc<1)||(nbeatc>PS_EDITSONG_BEAT_LIMIT)) return 0; // Try again.

  /* Truncate or expand song. */
  if (nbeatc!=WIDGET->beatc) {
    if (ps_sem_set_beat_count(WIDGET->sem,nbeatc)<0) return -1;
    WIDGET->beatc=nbeatc;
    WIDGET->dirty=1;
    if (ps_editsong_rebuild_songchart(widget)<0) return -1;
  }

  if (ps_widget_kill(dialogue)<0) return -1;
  return 0;
}

static int ps_editsong_cb_len(struct ps_widget *button,struct ps_widget *widget) {

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Song length in beats:",-1,
    WIDGET->beatc,
    ps_editsong_cb_len_ok
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;

  return 0;
}

/* Accessors.
 */
 
struct akau_song *ps_widget_editsong_get_song(const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return 0;
  return WIDGET->song;
}
 
struct ps_sem *ps_widget_editsong_get_sem(const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return 0;
  return WIDGET->sem;
}

int ps_widget_editsong_dirty(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;
  WIDGET->dirty=1;
  return 0;
}

/* Edit drum.
 */

static int ps_editsong_cb_edit_drum(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_widget *widget=ps_widget_dialogue_get_userdata(dialogue);
  int drumid=ps_widget_dialogue_get_refnum1(dialogue);
  struct akau_store *store=akau_get_store();
  ps_editsong_stop(widget);

  int ipcmid;
  if (ps_widget_dialogue_get_number(&ipcmid,dialogue)<0) return 0; // Bzzt, try again.
  if (!akau_store_get_ipcm(store,ipcmid)) return 0; // Bzzt, try again.
  if (akau_song_set_drum(WIDGET->song,drumid,ipcmid)<0) return 0; // Bzzt? Try again?
  if (akau_song_link(WIDGET->song,store)<0) return -1;
  if (ps_widget_kill(dialogue)<0) return -1;

  WIDGET->dirty=1;
  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  
  return 0;
}
 
int ps_widget_editsong_edit_drum(struct ps_widget *widget,int drumid) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;
  if (drumid<0) return -1;
  if (drumid>=akau_song_count_drums(WIDGET->song)) return -1;

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_number(
    ps_widget_get_root(widget),
    "Sound effect ID:",-1,
    akau_song_get_drum_ipcmid(WIDGET->song,drumid),
    ps_editsong_cb_edit_drum
  );
  if (!dialogue) return -1;

  if (ps_widget_dialogue_set_userdata(dialogue,widget,0)<0) return -1;
  if (ps_widget_dialogue_set_refnum1(dialogue,drumid)<0) return -1;
  
  return 0;
}

/* Edit instrument, callback from wave editor.
 */

static int ps_editsong_cb_edit_instrument(struct ps_widget *wave,struct ps_widget *widget) {
  if (!wave||!widget) return -1;
  int instrid=ps_widget_wave_get_refnum1(wave);

  /* Serialize. Log errors but report success. */
  char serial[42]; // 10+2*16 == 42; longest possible instrument
  int serialc=ps_widget_wave_serialize(serial,sizeof(serial),wave);
  if (serialc<0) {
    ps_log(EDIT,ERROR,"Failed to serialize instrument.");
    return 0;
  }
  if (serialc>sizeof(serial)) {
    ps_log(EDIT,ERROR,"Instrument serial length %d (expected no more than 42)",serialc);
    return 0;
  }

  //ps_log_hexdump(EDIT,DEBUG,serial,serialc,"Serialized instrument after editing:");

  /* Apply to song. */
  ps_editsong_stop(widget);
  if (akau_song_set_instrument(WIDGET->song,instrid,serial,serialc)<0) return -1;
  WIDGET->dirty=1;

  // No need to rebuild any UI.
  
  return 0;
}

/* Edit instrument.
 */

int ps_widget_editsong_edit_instrument(struct ps_widget *widget,int instrid) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;
  
  void *serial=0;
  int serialc=akau_song_get_instrument_source(&serial,WIDGET->song,instrid);
  if ((serialc<0)||!serial) {
    ps_log(EDIT,ERROR,"Failed to acquire serial source for instrument %d.",instrid);
    return 0;
  }

  //ps_log_hexdump(EDIT,DEBUG,serial,serialc,"Serialized instrument before editing:");

  struct ps_widget *wave=ps_widget_spawn(ps_widget_get_root(widget),&ps_widget_type_wave);
  if (!wave) return -1;

  if (ps_widget_wave_set_usage(wave,PS_WIDGET_WAVE_USAGE_INSTRUMENT)<0) return -1;
  if (ps_widget_wave_set_serial(wave,serial,serialc)<0) return -1;

  if (ps_widget_wave_set_callback(wave,ps_callback(ps_editsong_cb_edit_instrument,0,widget))<0) return -1;
  if (ps_widget_wave_set_refnum1(wave,instrid)<0) return -1;

  if (ps_widget_pack(ps_widget_get_root(widget))<0) return -1;
  
  return 0;
}

/* Confirm removal of drum or instrument.
 */

struct ps_editsong_userdata_delete_voice {
  struct ps_widget *widget;
  int voiceid;
  int (*cb_proceed)(struct ps_widget *widget,int voiceid,int force);
};

static int ps_editsong_cb_confirm_delete_voice(struct ps_widget *button,struct ps_widget *dialogue) {
  struct ps_editsong_userdata_delete_voice *userdata=ps_widget_dialogue_get_userdata(dialogue);
  if (!userdata) return -1;

  if (userdata->cb_proceed(userdata->widget,userdata->voiceid,1)<0) {
    return -1;
  }

  if (ps_widget_kill(dialogue)<0) return -1;
  
  return 0;
}

static int ps_editsong_confirm_delete_voice(
  struct ps_widget *widget,const char *voicetype,int voiceid,
  int (*cb_proceed)(struct ps_widget *widget,int voiceid,int force)
) {

  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"%s %d is in use. Really delete it?",voicetype,voiceid);
  if ((msgc<1)||(msgc>=sizeof(msg))) {
    strcpy(msg,"Really?");
    msgc=7;
  }

  struct ps_widget *dialogue=ps_widget_spawn_dialogue_message(
    ps_widget_get_root(widget),msg,msgc,ps_editsong_cb_confirm_delete_voice
  );
  if (!dialogue) return -1;

  struct ps_editsong_userdata_delete_voice *userdata=calloc(1,sizeof(struct ps_editsong_userdata_delete_voice));
  if (!userdata) return -1;
  userdata->widget=widget;
  userdata->voiceid=voiceid;
  userdata->cb_proceed=cb_proceed;
  if (ps_widget_dialogue_set_userdata(dialogue,userdata,free)<0) {
    free(userdata);
    return -1;
  }

  return 0;
}

/* Delete drum or instrument.
 */
 
int ps_widget_editsong_delete_drum(struct ps_widget *widget,int drumid,int force) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;

  ps_editsong_stop(widget);

  // Ensure (song) has the latest note data before we check voice usage.
  if (ps_sem_write_song(WIDGET->song,WIDGET->sem)<0) return -1;

  if (!force&&akau_song_drum_in_use(WIDGET->song,drumid)) {
    return ps_editsong_confirm_delete_voice(widget,"drum",drumid,ps_widget_editsong_delete_drum);
  }

  if (akau_song_remove_drum(WIDGET->song,drumid)<0) return -1;

  WIDGET->dirty=1;
  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;
  
  return 0;
}

int ps_widget_editsong_delete_instrument(struct ps_widget *widget,int instrid,int force) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;

  ps_editsong_stop(widget);

  // Ensure (song) has the latest note data before we check voice usage.
  if (ps_sem_write_song(WIDGET->song,WIDGET->sem)<0) return -1;

  if (!force&&akau_song_instrument_in_use(WIDGET->song,instrid)) {
    return ps_editsong_confirm_delete_voice(widget,"instrument",instrid,ps_widget_editsong_delete_instrument);
  }

  if (akau_song_remove_instrument(WIDGET->song,instrid)<0) return -1;

  WIDGET->dirty=1;
  if (ps_editsong_rebuild_voicelist(widget)<0) return -1;
  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

/* Get voice visibility.
 */
 
int ps_widget_editsong_get_voice_visibility(uint8_t *drums,uint8_t *instruments,const struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;
  const struct ps_widget *voicelist=ps_editsong_get_voicelist(widget);

  if (drums) memset(drums,0,32);
  if (instruments) memset(instruments,0,32);

  int i=voicelist->childc; while (i-->0) {
    struct ps_widget *songvoice=voicelist->childv[i];
    if (!ps_widget_songvoice_get_visibility(songvoice)) continue;
    uint8_t *buf=0;
    uint8_t id=ps_widget_songvoice_get_id(songvoice);
    switch (ps_widget_songvoice_get_type(songvoice)) {
      case 1: buf=drums; break;
      case 2: buf=instruments; break;
    }
    if (buf) {
      buf[id>>3]|=(1<<(id&7));
    }
  }

  return 0;
}

/* Report change in voice visibility.
 */
 
int ps_widget_editsong_voice_visibility_changed(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_editsong)) widget=widget->parent;
  if (ps_editsong_obj_validate(widget)<0) return -1;
  struct ps_widget *songchart=ps_editsong_get_songchart(widget);
  if (ps_widget_songchart_voice_visibility_changed(songchart)<0) return -1;
  return 0;
}
