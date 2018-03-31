#include "akau_global_internal.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

struct akau akau={0};

/* Main callback.
 */
 
static void akau_cb(int16_t *dst,int dstc) {
  if (akau_mixer_update(dst,dstc,akau.mixer)<0) {
    akau.error=1;
    memset(dst,0,dstc<<1);
  }
  int l=0,r=0;
  akau_mixer_get_clip(&l,&r,akau.mixer);
  if (akau.cliplc>INT_MAX-l) akau.cliplc=INT_MAX; else akau.cliplc+=l;
  if (akau.cliprc>INT_MAX-r) akau.cliprc=INT_MAX; else akau.cliprc+=r;
}

/* Init.
 */

int akau_init(
  const struct akau_driver *driver,
  void (*cb_log)(int loglevel,const char *msg,int msgc)
) {
  if (akau.init) return -1;
  
  if (!driver) return -1;
  if (!driver->init) return -1;
  if (!driver->quit) return -1;
  if (!driver->lock) return -1;
  if (!driver->unlock) return -1;

  memset(&akau,0,sizeof(struct akau));
  akau.init=1;
  memcpy(&akau.driver,driver,sizeof(struct akau_driver));
  akau.cb_log=cb_log;

  if (!(akau.mixer=akau_mixer_new())) {
    akau_quit();
    return -1;
  }

  if (!(akau.store=akau_store_new())) {
    akau_quit();
    return -1;
  }

  akau.rate=44100;
  const int chanc=2;
  if (akau.driver.init(akau.rate,chanc,akau_cb)<0) {
    akau_quit();
    return -1;
  }

  return 0;
}

/* Quit.
 */
 
void akau_quit() {
  if (!akau.init) return;

  if (akau.driver.quit) akau.driver.quit();

  akau_mixer_del(akau.mixer);
  akau_store_del(akau.store);

  if (akau.syncwatcherv) {
    while (akau.syncwatcherc>0) {
      struct akau_syncwatcher *syncwatcher=akau.syncwatcherv+(--(akau.syncwatcherc));
      if (syncwatcher->userdata_del) syncwatcher->userdata_del(syncwatcher->userdata);
    }
    free(akau.syncwatcherv);
  }

  if (akau.tokenv) free(akau.tokenv);

  memset(&akau,0,sizeof(struct akau));
}

/* Test initialization.
 */

int akau_is_init() {
  return akau.init;
}

/* Fire sync tokens.
 */

static int akau_fire_sync_token(uint16_t token) {
  const struct akau_syncwatcher *syncwatcher=akau.syncwatcherv;
  int i=akau.syncwatcherc; for (;i-->0;syncwatcher++) {
    int err=syncwatcher->cb(token,syncwatcher->userdata);
    if (err<0) return err;
  }
  return 0;
}

/* Main thread update.
 */
 
int akau_update() {
  if (!akau.init) return 0;

  /* Report deferred errors. */
  if (akau.error) {
    akau.error=0;
    return -1;
  }

  /* Fire synchronization tokens. */
  if (akau.tokenc) {
    if (akau.driver.lock()<0) return -1;
    int i; for (i=0;i<akau.tokenc;i++) {
      int err=akau_fire_sync_token(akau.tokenv[i]);
      if (err<0) {
        akau.driver.unlock();
        return err;
      }
    }
    akau.tokenc=0;
    if (akau.driver.unlock()<0) return -1;
  }
  
  /* Fire clip callback if needed. */
  if (akau.cb_clip) {
    if (akau.cliplc||akau.cliprc) {
      int err=akau.cb_clip(akau.cliplc,akau.cliprc);
      if (err<0) return err;
      akau.cliplc=0;
      akau.cliprc=0;
    }
  } else {
    akau.cliplc=0;
    akau.cliprc=0;
  }
  
  return 0;
}

/* Play sound or song.
 */
 
int akau_play_sound_as(int ipcmid,uint8_t trim,int8_t pan,uint8_t intent) {
  if (!akau.init) return -1;
  struct akau_ipcm *ipcm=akau_store_get_ipcm(akau.store,ipcmid);
  if (!ipcm) return -1;
  return akau_mixer_play_ipcm(akau.mixer,ipcm,trim,pan,0,intent);
}
 
int akau_play_loop_as(int ipcmid,uint8_t trim,int8_t pan,uint8_t intent) {
  if (!akau.init) return -1;
  struct akau_ipcm *ipcm=akau_store_get_ipcm(akau.store,ipcmid);
  if (!ipcm) return -1;
  return akau_mixer_play_ipcm(akau.mixer,ipcm,trim,pan,1,intent);
}

int akau_play_song_as(int songid,int restart,uint8_t intent) {
  if (!akau.init) return -1;
  struct akau_song *song=0;
  if (songid) {
    song=akau_store_get_song(akau.store,songid);
    if (!song) return -1;
  }
  return akau_mixer_play_song(akau.mixer,song,restart,intent);
}

/* Intent.
 */
 
int akau_play_sound(int ipcmid,uint8_t trim,int8_t pan) {
  return akau_play_sound_as(ipcmid,trim,pan,AKAU_INTENT_SFX);
}

int akau_play_loop(int ipcmid,uint8_t trim,int8_t pan) {
  return akau_play_sound_as(ipcmid,trim,pan,AKAU_INTENT_SFX);
}

int akau_play_song(int songid,int restart) {
  return akau_play_song_as(songid,restart,AKAU_INTENT_BGM);
}
 
int akau_set_trim_for_intent(uint8_t intent,uint8_t trim) {
  return akau_mixer_set_trim_for_intent(akau.mixer,intent,trim);
}

uint8_t akau_get_trim_for_intent(uint8_t intent) {
  return akau_mixer_get_trim_for_intent(akau.mixer,intent);
}

/* Lock.
 */

int akau_lock() {
  if (!akau.init) return -1;
  if (!akau.driver.lock) return -1;
  return akau.driver.lock();
}

int akau_unlock() {
  if (!akau.init) return -1;
  if (!akau.driver.unlock) return -1;
  return akau.driver.unlock();
}

/* Trivial accessors.
 */
 
struct akau_mixer *akau_get_mixer() {
  return akau.mixer;
}

struct akau_store *akau_get_store() {
  return akau.store;
}

int akau_load_resources(const char *path) {
  if (!akau.init) return -1;
  return akau_store_load(akau.store,path);
}

int akau_set_clip_callback(int (*cb)(int cliplc,int cliprc)) {
  if (!akau.init) return -1;
  akau.cb_clip=cb;
  return 0;
}

/* Logging.
 */
 
const char *akau_loglevel_repr(int loglevel) {
  switch (loglevel) {
    case AKAU_LOGLEVEL_DEBUG: return "DEBUG";
    case AKAU_LOGLEVEL_INFO: return "INFO";
    case AKAU_LOGLEVEL_WARN: return "WARN";
    case AKAU_LOGLEVEL_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

static void akau_log(int loglevel,const char *fmt,va_list vargs) {
  if (akau.init&&!akau.cb_log) return; // Logging deliberately suppressed by client.
  if (!fmt) fmt="";
  char buf[256];
  int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
  if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
  if (akau.init) {
    akau.cb_log(loglevel,buf,bufc);
  } else {
    // For logging purposes, the library need not be initialized.
    fprintf(stderr,"akau:%s: %.*s\n",akau_loglevel_repr(loglevel),bufc,buf);
  }
}

int akau_error(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  akau_log(AKAU_LOGLEVEL_ERROR,fmt,vargs);
  return -1;
}

int akau_warn(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  akau_log(AKAU_LOGLEVEL_WARN,fmt,vargs);
  return 0;
}

int akau_info(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  akau_log(AKAU_LOGLEVEL_INFO,fmt,vargs);
  return 0;
}

int akau_debug(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  akau_log(AKAU_LOGLEVEL_DEBUG,fmt,vargs);
  return 0;
}

/* Get master output rate, assume 44100 if unset.
 */

int akau_get_master_rate() {
  if (akau.rate>0) return akau.rate;
  return 44100;
}

/* Add syncwatcher.
 */

static int akau_syncwatcher_id_in_use(int id) {
  const struct akau_syncwatcher *syncwatcher=akau.syncwatcherv;
  int i=akau.syncwatcherc; for (;i-->0;syncwatcher++) {
    if (syncwatcher->id==id) return 1;
  }
  return 0;
}

static int akau_syncwatcher_unique_id() {
  if (akau.syncwatcherc<1) return 1;
  int id=akau.syncwatcherv[akau.syncwatcherc-1].id;
  if (id<INT_MAX) id++;
  while (1) {
    // This will typically work on the first try.
    // If it does need to loop, it will definitely find a gap somewhere.
    // We know this, because there are (INT_MAX) possible IDs, and (sizeof(struct akau_syncwatcher)>1).
    if (!akau_syncwatcher_id_in_use(id)) return id;
    if (id==INT_MAX) id=1;
    else id++;
  }
}
 
int akau_watch_sync_tokens(
  int (*cb)(uint16_t token,void *userdata),
  void (*userdata_del)(void *userdata),
  void *userdata
) {
  if (!akau.init) return -1;
  if (!cb) return -1;

  if (akau.syncwatcherc>=akau.syncwatchera) {
    int na=akau.syncwatchera+8;
    if (na>INT_MAX/sizeof(struct akau_syncwatcher)) return -1;
    void *nv=realloc(akau.syncwatcherv,sizeof(struct akau_syncwatcher)*na);
    if (!nv) return -1;
    akau.syncwatcherv=nv;
    akau.syncwatchera=na;
  }

  int id=akau_syncwatcher_unique_id();

  struct akau_syncwatcher *syncwatcher=akau.syncwatcherv+akau.syncwatcherc++;
  syncwatcher->id=id;
  syncwatcher->cb=cb;
  syncwatcher->userdata_del=userdata_del;
  syncwatcher->userdata=userdata;

  return id;
}

int akau_unwatch_sync_tokens(int syncwatcherid) {
  int i=0;
  struct akau_syncwatcher *syncwatcher=akau.syncwatcherv;
  for (;i<akau.syncwatcherc;i++,syncwatcher++) {
    if (syncwatcher->id!=syncwatcherid) continue;
    if (syncwatcher->userdata_del) syncwatcher->userdata_del(syncwatcher->userdata);
    akau.syncwatcherc--;
    memmove(syncwatcher,syncwatcher+1,sizeof(struct akau_syncwatcher)*(akau.syncwatcherc-i));
    return 0;
  }
  return -1;
}

/* Queue sync token.
 */
 
int akau_queue_sync_token(uint16_t token) {
  if (!akau.init) return -1;
  
  if (akau.tokenc>=akau.tokena) {
    int na=akau.tokena+32;
    if (na>INT_MAX/sizeof(uint16_t)) return -1;
    void *nv=realloc(akau.tokenv,sizeof(uint16_t)*na);
    if (!nv) return -1;
    akau.tokenv=nv;
    akau.tokena=na;
  }

  akau.tokenv[akau.tokenc++]=token;

  return 0;
}
