/* akau_song.h
 * Defines a sequence of commands to deliver to mixer.
 * Also serves as a store for instruments and sounds.
 */

#ifndef AKAU_SONG_H
#define AKAU_SONG_H

#include <stdint.h>

struct akau_song;
struct akau_instrument;
struct akau_ipcm;
struct akau_mixer;
struct akau_song_decoder;
struct akau_store;

struct akau_song *akau_song_new();
void akau_song_del(struct akau_song *song);
int akau_song_ref(struct akau_song *song);

/* A locked song is immutable.
 * Each lock must be matched by one unlock.
 */
int akau_song_lock(struct akau_song *song);
int akau_song_unlock(struct akau_song *song);

/* Add data objects to song.
 * Each instrument and each ipcm gets a unique ID, 0..255.
 * We return that ID. They are guaranteed to be assigned sequentially.
 * (instrument IDs and ipcm IDs are separate)
 */
int akau_song_add_instrument(struct akau_song *song,struct akau_instrument *instrument);
int akau_song_add_ipcm(struct akau_song *song,struct akau_ipcm *ipcm);

/* Linkage.
 * Instruments and ipcms can be pulled from an external store.
 * They need not exist at decode time, but must be discoverable by link time.
 * So, after loading all resources, you should link each song.
 * An unlinked song can not be locked and therefore can not be played by a mixer.
 * Linking with a null store is legal if no external references were declared.
 */
int akau_song_add_external_instrument(struct akau_song *song,int instrumentid);
int akau_song_add_external_ipcm(struct akau_song *song,int ipcmid);
int akau_song_link(struct akau_song *song,struct akau_store *store);

/* Add commands to song.
 */
int akau_song_addcmd_delay(struct akau_song *song,int framec);
int akau_song_addcmd_sync(struct akau_song *song,uint16_t token);
int akau_song_addcmd_note(struct akau_song *song,uint8_t instrid,uint8_t pitch,uint8_t trim,int8_t pan,int32_t framec);
int akau_song_addcmd_drum(struct akau_song *song,uint8_t ipcmid,uint8_t trim,int8_t pan);

/* Consider the given command position, run all commands from there through the first DELAY.
 * Apply commands to the given mixer.
 */
int akau_song_update(struct akau_song *song,struct akau_mixer *mixer,int cmdp);

/* Decode text into a new song.
 * This is a convenience that prevents you from needing a decoder object.
 */
struct akau_song *akau_song_decode(
  const char *src,int srcc,
  void (*cb_error)(const char *msg,int msgc,const char *src,int srcc,int srcp)
);

struct akau_song_decoder *akau_song_decoder_new();
void akau_song_decoder_del(struct akau_song_decoder *decoder);
int akau_song_decoder_ref(struct akau_song_decoder *decoder);

struct akau_song *akau_song_decoder_decode(
  struct akau_song_decoder *decoder,
  const char *src,int srcc,
  void (*cb_error)(const char *msg,int msgc,const char *src,int srcc,int srcp)
);

/******************************************************************************
 * ENCODED SONG FORMAT
 *-----------------------------------------------------------------------------
 * Line-oriented text.
 * Three sections: Definitions, Channels, Body
 * Sections are separated by a line beginning with dash (rest of that line ignored).
 * Hash begins a line comment only if it is the first non-whitespace character on the line.
 * 
 * === Definitions Section ===
 * Line is split on whitespace; the first word identifies the command:
 *   speed INTEGER
 *     Song's speed in beats per minute. Each line in the body is one beat.
 *   instrument ...
 *     Instrument definition, see below.
 *   drum ...
 *     Drum definition, see below.
 *
 * Instruments are assigned IDs sequentially from zero, in the order they appear.
 * Instrument definition may be an external resource reference:
 *   instrument external RESID
 * Otherwise the remainder of the line is delivered to akau_instrument_decode().
 *
 * Drums are assigned IDs sequentially from zero, in the order they appear.
 * Drum definition may be an external resource reference:
 *   drum external RESID
 * I don't think it will be possible to succinctly define a useful drum, so these are only "external".
 *
 * === Channels Section ===
 * Line is split on whitespace; each line must contain the same word count.
 * One word per channel on each line.
 * Either "KEY", "KEY=VALUE", or ".".
 * Channel keys:
 *   pan (-128..127)
 *   trim (0..255)
 *   instrument (0..255)
 *   ins -- Alias for "instrument"
 *   drums -- No value; flag this as a drum channel.
 *
 * === Body Section ===
 * Line is split on whitespace; each line has one word per channel.
 * If the word count is short, rightward channels are assumed no-op.
 *
 * For all channels:
 *   .
 *     No-op.
 *   
 * For drum channels:
 *   DRUMID [TRIM] [PAN]
 *     Play the given drum, optionally overriding default trim and pan.
 *
 * For instrument channels:
 *   [INSTRUMENTID] PITCH [DURATION] [TRIM] [PAN]
 *     Play a note, optionally overriding default instrument, trim, and pan.
 *   eg: a4 c#10 (3)g5=2:50%@-20~
 *   Where instrument, trim, or pan is omitted, we use the channel default (not necessarily the most recent).
 *
 * The channel concept is a notational convenience.
 * It is perfectly legal for multiple notes to be playing on one channel at the same time.
 *
 * DRUMID: Decimal integer.
 * TRIM: Colon, integer, '%' if relative to channel default.
 * PAN: At, integer, '~' if relative to channel default.
 * INSTRUMENTID: Parenthesized integer.
 * PITCH: Name, modifier, octave. eg "a4" "c#0" "gb10".
 * DURATION: Equal, beat count. Default 1.
 *
 * Body lines may also be a synchronization command: sync TOKEN
 * TOKEN is a decimal integer 0..65535.
 * This token will fire at the beginning of the next beat.
 *
 *****************************************************************************/

#endif
