/* ps_isong.h
 * Intermediate song format.
 * This is the editor's data model.
 * It easily converts to binary, assembly, or a live akau_song.
 * TODO The format and command set is a little different from akau; I plan to modify akau a little.
 */

#ifndef PS_ISONG_H
#define PS_ISONG_H

#define PS_ISONG_OP_NOOP      0x00
#define PS_ISONG_OP_BEAT      0x01 /* Begin the next beat. */
#define PS_ISONG_OP_NOTE      0x02 /* Play tuned instrument. */
#define PS_ISONG_OP_DRUM      0x03 /* Play verbatim wave. */
#define PS_ISONG_OP_ASSIGN    0x04 /* Store the channel of the last NOTE command. */
#define PS_ISONG_OP_GLISS     0x05 /* Bend pitch of a note. */

struct ps_isong_instrument {
  char *text;
  int textc;
};

struct ps_isong_command {
  uint8_t op;
  struct { uint8_t op; uint32_t index; } BEAT; // (index) is transient, we don't store it.
  struct { uint8_t op; uint8_t instrid,pitch,trim,duration; int8_t pan; } NOTE;
  struct { uint8_t op; uint8_t drumid,trim; int8_t pan; } DRUM;
  struct { uint8_t op; uint8_t chanid; } ASSIGN;
  struct { uint8_t op; uint8_t chanid,pitch,duration; } GLISS;
};

struct ps_isong {
  int refc;
  uint16_t tempo; // bpm
  int *drumidv; // ipcm ids
  int drumidc,drumida;
  struct ps_isong_instrument *instrv;
  int instrc,instra;
  struct ps_isong_command *cmdv;
  int cmdc,cmda;
};

struct ps_isong *ps_isong_new();
void ps_isong_del(struct ps_isong *isong);
int ps_isong_ref(struct ps_isong *isong);

int ps_isong_clear(struct ps_isong *isong);

/* Removing a drum or instrument cascades into commands.
 */
int ps_isong_add_drum(struct ps_isong *isong,int ipcmid);
int ps_isong_remove_drum(struct ps_isong *isong,int drumid);
int ps_isong_add_instrument(struct ps_isong *isong,const char *text,int textc);
int ps_isong_remove_instrument(struct ps_isong *isong,int instrid);

/* Does this look like valid instrument text?
 */
int ps_isong_validate_instrument(const char *src,int srcc);

/* The command list is structured loosely by BEAT commands.
 * Each BEAT has a sequential index, which we keep up to date.
 * The first command in a song is always BEAT with index==0.
 */
int ps_isong_count_beats(const struct ps_isong *isong);
int ps_isong_find_beat(const struct ps_isong *isong,int beatp); // => cmdp
int ps_isong_beat_from_cmd(const struct ps_isong *isong,int cmdp); // => beatp
int ps_isong_beat_cmdp_from_cmd(const struct ps_isong *isong,int cmdp); // => cmdp
int ps_isong_measure_beat(const struct ps_isong *isong,int cmdp); // => cmdc to next BEAT or end

/* Remove multiple commands.
 * We update BEAT indices as necessary.
 */
int ps_isong_remove_commands(struct ps_isong *isong,int cmdp,int cmdc);

/* Add a command.
 * Caller then populates it.
 * DO NOT set op to BEAT; those are special.
 * (cmdp<0) to append.
 */
struct ps_isong_command *ps_isong_insert_command(struct ps_isong *isong,int cmdp);

/* Insert BEAT commands and update indices.
 * (cmdp<0) to append.
 */
int ps_isong_insert_beats(struct ps_isong *isong,int cmdp,int beatc);

/* Increase resolution of the song by adding dummy beats, increasing NOTE and GLISS durations, and increasing tempo.
 * If this works, there should be no difference in playback.
 */
int ps_isong_split_beats(struct ps_isong *isong,int multiplier);

/* Reverse of ps_isong_split_beats().
 * Keep only the first of every (factor) beats, reduce command durations, and reduce tempo.
 * This is always safe immediately after a split, but otherwise can be extremely destructive.
 * Use (force==0) to proceed only if no data will be lost.
 */
int ps_isong_merge_beats(struct ps_isong *isong,int factor,int force);

/* I envision three serial formats:
 *   - "binary" is preferred for distribution.
 *   - "assembly" is an analogue of "binary" but more or less human-readable.
 *   - "text" is the existing akau format. Not a priority to support this one, I think.
 * We'll decode all formats automagically.
 */
int ps_isong_encode_binary(void *dstpp,const struct ps_isong *isong);
int ps_isong_encode_assembly(void *dstpp,const struct ps_isong *isong);
int ps_isong_encode_text(void *dstpp,const struct ps_isong *isong);
int ps_isong_decode(struct ps_isong *isong,const void *src,int srcc);

/* Copy to AKAU format ready for playback.
 */
struct akau_song *ps_song_from_isong(const struct ps_isong *isong);
struct ps_isong *ps_isong_from_song(const struct akau_song *song);

#endif
