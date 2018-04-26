/* akau_mixer.h
 * Manages playback of tuned and verbatim waves.
 * Every note or sampled sound you play creates a new temporary channel.
 * You can refer to those channels by ID later to adjust pitch, trim, and pan.
 * Channel IDs are reused eventually, but it should be centuries later.
 */

#ifndef AKAU_MIXER_H
#define AKAU_MIXER_H

#include <stdint.h>

struct akau_mixer;
struct akau_instrument;
struct akau_ipcm;
struct akau_song;

struct akau_mixer *akau_mixer_new();
void akau_mixer_del(struct akau_mixer *mixer);
int akau_mixer_ref(struct akau_mixer *mixer);

/* Update all running channels into the given buffer.
 * Any prior content in the buffer is obliterated.
 * (dst) contains samples arranged L,R,L,R,etc.
 * (dstc) is the count of samples to produce, not frames, not bytes.
 */
int akau_mixer_update(int16_t *dst,int dstc,struct akau_mixer *mixer);

/* Get the clip counts per channel, and reset them internally.
 */
int akau_mixer_get_clip(int *l,int *r,struct akau_mixer *mixer);

/* Mixers are stereo by default.
 * Songprinter needs to turn this off for its private mixer.
 */
int akau_mixer_set_stereo(struct akau_mixer *mixer,int stereo);

int akau_mixer_count_channels(const struct akau_mixer *mixer);

/* Begin a musical note with the given instrument and parameters.
 * After (duration) frames, it will begin decaying to silence.
 * (instrument) presumably contains a wave sampled at 1 Hz.
 * Returns a channel ID so you can modify in flight later.
 */
int akau_mixer_play_note(
  struct akau_mixer *mixer,
  struct akau_instrument *instrument,
  uint8_t pitch,
  uint8_t trim,
  int8_t pan,
  int duration,
  uint8_t intent
);

/* Begin playing a sampled sound without adjusting output rate.
 * If (loop) nonzero, (ipcm) must contain loop control points and we will repeat it forever.
 * Returns a channel ID so you can modify in flight later.
 * If looping, you are expected to retain that ID somewhere and eventually stop it.
 */
int akau_mixer_play_ipcm(
  struct akau_mixer *mixer,
  struct akau_ipcm *ipcm,
  uint8_t trim,
  int8_t pan,
  int loop,
  uint8_t intent
);

/* Get properties for a given channel.
 */
int akau_mixer_get_channel(
  uint8_t *pitch,uint8_t *trim,int8_t *pan,
  const struct akau_mixer *mixer,
  int chanid
);

/* Update properties for a given channel.
 * (duration) is the count of frames over which to effect this change.
 * If too low, you may produce popping artifacts.
 */
int akau_mixer_adjust_channel(
  struct akau_mixer *mixer,
  int chanid,
  uint8_t pitch,uint8_t trim,int8_t pan,
  int duration
);

/* Start winding down whatever is playing on this channel.
 * If it's an instrument, enter the decay phase immediately.
 * If it's a PCM loop, flag the channel to exit the loop at the next pass.
 * If it's a non-looping PCM channel, do nothing; it will end itself eventually.
 */
int akau_mixer_stop_channel(struct akau_mixer *mixer,int chanid);

/* Begin fading out all channels, and mark them to terminate when we reach silence.
 * This is the graceful way to cut off all output, if your updates are still running.
 * 'stop_all_instruments' ignores IPCM channels, maybe nice for ending music but not sound effects.
 */
int akau_mixer_stop_all(struct akau_mixer *mixer,int duration);
int akau_mixer_stop_all_instruments(struct akau_mixer *mixer,int duration);

/* Set a song to play automatically by this mixer.
 * (song) may be null to end playback.
 * If (song) is currently playing, we either do nothing (restart==0) or restart it (restart!=0).
 * We always lock songs while they are playing.
 */
int akau_mixer_play_song(struct akau_mixer *mixer,struct akau_song *song,int restart,uint8_t intent);

/* Stop whatever is playing and start this song at the given beat.
 */
int akau_mixer_play_song_from_beat(struct akau_mixer *mixer,struct akau_song *song,int beatp,uint8_t intent);

/* Set or get a trim applying to arbitrarily-grouped channels.
 * So you can provide your users eg "sound effects level" and "music level".
 */
int akau_mixer_set_trim_for_intent(struct akau_mixer *mixer,uint8_t intent,uint8_t trim);
uint8_t akau_mixer_get_trim_for_intent(const struct akau_mixer *mixer,uint8_t intent);

/* For song support only.
 * These should only be called from within akau_song_update().
 * Delay is the count of frames before we will update the song again.
 * Position is the next 'cmdp' we will deliver to song update.
 * A song must call both of these during its update, or it will get the same update request one frame later.
 */
int akau_mixer_set_song_delay(struct akau_mixer *mixer,int framec);
int akau_mixer_set_song_position(struct akau_mixer *mixer,int cmdp);

#endif
