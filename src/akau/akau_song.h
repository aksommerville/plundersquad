/* akau_song.h
 * Playback sequencer.
 * Delivers notes and verbatim sounds to mixer on schedule.
 *
 * Pitches reference:
 *   0x00  a0   27.5
 *   0x0c  a1   55
 *   0x18  a2   110
 *   0x24  a3   220
 *   0x30  a4   440
 *   0x3c  a5   880
 *   0x48  a6   1760
 *   0x54  a7   3520
 *   0x60  a8   7040
 *   0x6c  a9   14080
 *   0x78  a10  28160
 */

#ifndef AKAU_SONG_H
#define AKAU_SONG_H

#include <stdint.h>

struct akau_song;
struct akau_instrument;
struct akau_ipcm;
struct akau_mixer;
struct akau_store;

#define AKAU_SONG_OP_NOOP      0x00 /* Dummy, do nothing. */
#define AKAU_SONG_OP_BEAT      0x01 /* End of beat. */
#define AKAU_SONG_OP_DRUM      0x02 /* Begin verbatim sound. */
#define AKAU_SONG_OP_NOTE      0x03 /* Begin tuned sound. */
#define AKAU_SONG_OP_ADJPITCH  0x04 /* Begin pitch adjustment. */
#define AKAU_SONG_OP_ADJTRIM   0x05 /* Begin trim adjustment. */
#define AKAU_SONG_OP_ADJPAN    0x06 /* Begin pan adjustment. */

union akau_song_command {
  uint8_t op;
  struct { uint8_t op; uint8_t ref,drumid,trim,pan; } DRUM;
  struct { uint8_t op; uint8_t ref,instrid,trim,pan,pitch,duration; } NOTE;
  struct { uint8_t op; uint8_t ref,v,duration; } ADJ;
};

struct akau_song *akau_song_new();
void akau_song_del(struct akau_song *song);
int akau_song_ref(struct akau_song *song);

int akau_song_clear(struct akau_song *song);

/* A song must be locked to play and unlocked to edit.
 * Locking performs a quick consistency check and fails if it doesn't look playable.
 * eg a brand-new song will refuse to lock.
 */
int akau_song_get_lock(const struct akau_song *song); // => 0,1. 0 if null
int akau_song_lock(struct akau_song *song);
int akau_song_unlock(struct akau_song *song);

/* The song has a global tempo in beats per minute.
 * The "beat" is the minimum resolution of all timing.
 * It's usually some multiple of the desired musical tempo.
 */
int akau_song_get_tempo(const struct akau_song *song);
int akau_song_set_tempo(struct akau_song *song,int tempo_bpm);

/* Drums are IPCM resources from the global store.
 * We preserve the ID, and also keep a reference to the IPCM object.
 * It is legal to have ID only, if the song can be put in a pre-link state.
 * After setting a drum by ipcmid, you need to relink the song.
 * Deleting a drum also deletes any command referring to it, and updates the IDs of other commands to reflect the new list.
 */
int akau_song_count_drums(const struct akau_song *song);
struct akau_ipcm *akau_song_get_drum(const struct akau_song *song,int drumid);
int akau_song_get_drum_ipcmid(const struct akau_song *song,int drumid);
int akau_song_set_drum(struct akau_song *song,int drumid,int ipcmid);
int akau_song_add_drum(struct akau_song *song,int ipcmid); // => drumid (sequential from zero).
int akau_song_remove_drum(struct akau_song *song,int drumid);
int akau_song_drum_in_use(const struct akau_song *song,int drumid); // => nonzero if any command refers to it

/* Instruments are private to the song, they do not have external linkage.
 * (AKAU does have an "instrument" resource type, but we don't use it).
 * We preserve the instrument object and the serialized source of it.
 * Deleting a drum also deletes any command referring to it, and updates the IDs of other commands to reflect the new list.
 */
int akau_song_count_instruments(const struct akau_song *song);
struct akau_instrument *akau_song_get_instrument(const struct akau_song *song,int instrid);
int akau_song_get_instrument_source(void *dstpp_READONLY,const struct akau_song *song,int instrid);
int akau_song_set_instrument(struct akau_song *song,int instrid,const void *src,int srcc);
int akau_song_add_instrument(struct akau_song *song,const void *src,int srcc); // => instrid (sequential from zero).
int akau_song_remove_instrument(struct akau_song *song,int instrid);
int akau_song_instrument_in_use(const struct akau_song *song,int instrid); // => nonzero if any command refers to it

/* The important part of a song is its command list.
 * These are framed internally by BEAT commands, one at the end of each beat.
 * Getting or setting a command copies its content.
 */
int akau_song_count_commands(const struct akau_song *song);
int akau_song_get_command(union akau_song_command *dst,const struct akau_song *song,int cmdp);
int akau_song_set_command(struct akau_song *song,int cmdp,const union akau_song_command *src);
int akau_song_insert_command(struct akau_song *song,int cmdp,const union akau_song_command *src);
int akau_song_remove_commands(struct akau_song *song,int cmdp,int cmdc);
int akau_song_get_all_commands(union akau_song_command **dstpp,const struct akau_song *song); // Get the internal list, handle with care.

/* Convert between beat index and command index.
 * These could be expensive.
 * Editors are advised to record their command positions where possible; 'advance' and 'retreat' are cheaper.
 */
int akau_song_beatp_from_cmdp(const struct akau_song *song,int cmdp);
int akau_song_cmdp_from_beatp(const struct akau_song *song,int beatp); // => Index of first command of beat.
int akau_song_cmdp_advance(const struct akau_song *song,int cmdp); // => Index of first command of next beat.
int akau_song_cmdp_retreat(const struct akau_song *song,int cmdp); // => Index of first command of previous beat.
int akau_song_count_beats(const struct akau_song *song);

/* Eliminate commands or add empty beats to reach the given beat count.
 */
int akau_song_adjust_length(struct akau_song *song,int beatc);

/* We serialize to a private binary format.
 *   0000   8 Signature: "\0AK\xffSONG"
 *   0008   2 Tempo, bpm.
 *   000a   1 Drum count.
 *   000b   1 Instrument count.
 *   000c   4 Extra header length.
 *   0010 ... Extra header.
 *   .... ... Drums:
 *     0000   2 IPCM ID.
 *     0002
 *   .... ... Instruments:
 *     0000   1 Length.
 *     0001 ... Serialized instrument (binary format).
 *     ....
 *   .... ... Commands, see union akau_song_command.
 */
int akau_song_encode(void *dstpp,const struct akau_song *song);
int akau_song_decode(struct akau_song *song,const void *src,int srcc);

/* Resolve references to IPCM objects.
 * Linkage must be complete before we permit locking (ie playback).
 */
int akau_song_link(struct akau_song *song,struct akau_store *store);

/* Play commands beginning at (cmdp) through the next BEAT, into the given mixer.
 * We call the mixer to set its next delay and position.
 */
int akau_song_update(struct akau_song *song,struct akau_mixer *mixer,int cmdp);

int akau_song_set_sync_callback(struct akau_song *song,int (*cb_sync)(struct akau_song *song,int beatp,void *userdata),void *userdata);
void *akau_song_get_userdata(const struct akau_song *song);
int akau_song_restart(struct akau_song *song);
int akau_song_restart_at_beat(struct akau_song *song,int beatp);

#endif
