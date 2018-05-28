/* akau.h
 */

#ifndef AKAU_H
#define AKAU_H

#include <stdint.h>

#include "akau_driver.h"
#include "akau_pcm.h"
#include "akau_wavegen.h"
#include "akau_instrument.h"
#include "akau_song.h"
#include "akau_mixer.h"
#include "akau_store.h"

#define AKAU_LOGLEVEL_DEBUG       1
#define AKAU_LOGLEVEL_INFO        2
#define AKAU_LOGLEVEL_WARN        3
#define AKAU_LOGLEVEL_ERROR       4
const char *akau_loglevel_repr(int loglevel);

/* "Intent" is a grouping of related sounds, which can share a global trim.
 * AKAU internally may produce OTHER, SFX, or BGM (0,1,2).
 * Any other intent is for your own use.
 * Legal values are 0..255.
 */
#define AKAU_INTENT_OTHER     0
#define AKAU_INTENT_SFX       1
#define AKAU_INTENT_BGM       2
#define AKAU_INTENT_FOLEY     3
#define AKAU_INTENT_UI        4
#define AKAU_INTENT_VOICE     5

int akau_init(
  const struct akau_driver *driver,
  void (*cb_log)(int loglevel,const char *msg,int msgc)
);

void akau_quit();

int akau_is_init();

/* Set a callback to fire during akau_update() if any clipping happened on master.
 * Only one such callback is permitted.
 */
int akau_set_clip_callback(int (*cb)(int cliplc,int cliprc));

/* See akau_store_load().
 */
int akau_load_resources(const char *path);

/* The real updating happens in a callback on another thread.
 * This routine update is only for error reporting, song synchronization, and clip reporting.
 */
int akau_update();

/* Find a sampled sound effect in the global store and begin playing it.
 * "intent" was shoe-horned in late, and I didn't want to break the existing interface.
 * Default intent is 1 (AKAU_INTENT_SFX), or use the "_as" versions for a specific intent.
 */
int akau_play_sound(int ipcmid,uint8_t trim,int8_t pan);
int akau_play_loop(int ipcmid,uint8_t trim,int8_t pan);
int akau_play_sound_as(int ipcmid,uint8_t trim,int8_t pan,uint8_t intent);
int akau_play_loop_as(int ipcmid,uint8_t trim,int8_t pan,uint8_t intent);

/* Find a song in the global store and replace the current song with it.
 * Use (songid==0) to play silence.
 * Use (restart!=0) if this might be the current song and you want it to play from the beginning.
 */
int akau_play_song(int songid,int restart);
int akau_play_song_as(int songid,int restart,uint8_t intent);

/* Driver guarantees that the callback is not running while we hold the lock.
 * With that in mind, don't ever hold it very long.
 * The functions in this global header all manage the lock for you.
 * Type-specific functions do not.
 */
int akau_lock();
int akau_unlock();

int akau_set_trim_for_intent(uint8_t intent,uint8_t trim);
uint8_t akau_get_trim_for_intent(uint8_t intent);

/* Return the shared global mixer.
 * You should definitely hold the lock if changing anything in it.
 */
struct akau_mixer *akau_get_mixer();

/* Return the global shared resource store.
 */
struct akau_store *akau_get_store();

/* Register a callback to be notified when the song hits a SYNC command.
 * These callbacks will fire from the main thread, during akau_update().
 * Returns (syncwatcherid), which you can use later to remove the watcher. (always >0)
 */
int akau_watch_sync_tokens(
  int (*cb)(uint16_t token,void *userdata),
  void (*userdata_del)(void *userdata),
  void *userdata
);

int akau_unwatch_sync_tokens(int syncwatcherid);

/* For use by mixer.
 */
int akau_queue_sync_token(uint16_t token);

/* Register a callback to fire inside the main callback between mixer and delivery.
 * We do not provide any cleanup for (userdata).
 * There can be only one observer, if there's competition for this that's your problem.
 */
int akau_set_observer(
  void (*cb)(const int16_t *v,int c,void *userdata),
  void *userdata
);

#endif
