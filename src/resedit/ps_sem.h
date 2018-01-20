/* ps_sem.h
 * Song Editor Model.
 * This is a close analogue of akau_song, which is designed for use in the editor.
 * You will have both objects: an akau_song and a ps_sem.
 * The SEM is a memory hog, and very redundant internally. That's by design.
 */

#ifndef PS_SEM_H
#define PS_SEM_H

struct akau_song;

/* Special opcodes that won't exist in the song.
 * We don't use the 'duration' parameter of any command.
 * Instead there are sentinel 'END' commands.
 * ENDs are inclusive; it is sensible for one to occur on the same beat as its start.
 * For adjustments, the initial command has a redundant value and the sentinel has the target value.
 */
#define PS_SEM_OP_NOTEEND     0xf0
#define PS_SEM_OP_ADJPITCHEND 0xf1
#define PS_SEM_OP_ADJTRIMEND  0xf2
#define PS_SEM_OP_ADJPANEND   0xf3

struct ps_sem_event {
  uint8_t op;
  int noteid; // Every NOTE command has a unique ID, referenced by its adjustments and sentinel.
  uint8_t objid; // drumid or instrid -- Adjustments and sentinels must set this too!
  uint8_t pitch;
  uint8_t trim;
  int8_t pan;
};

struct ps_sem_beat {
  struct ps_sem_event *eventv;
  int eventc,eventa;
};

struct ps_sem {
  int refc;
  struct ps_sem_beat *beatv;
  int beatc,beata;
  int noteid_next;
};

struct ps_sem *ps_sem_new();
void ps_sem_del(struct ps_sem *sem);
int ps_sem_ref(struct ps_sem *sem);

int ps_sem_remove_beats(struct ps_sem *sem,int p,int c);
int ps_sem_insert_beats(struct ps_sem *sem,int p,int c);
int ps_sem_set_beat_count(struct ps_sem *sem,int beatc);

/* These are low-level operations used internally.
 */
struct ps_sem_event *ps_sem_beat_new_event(struct ps_sem_beat *beat);
int ps_sem_beat_remove_events(struct ps_sem_beat *beat,int p,int c);

/* Add an event.
 * Adding note place both NOTE and NOTEEND events, both on this beat.
 * Adding note returns the new noteid.
 */
int ps_sem_add_note(struct ps_sem *sem,int beatp,uint8_t instrid,uint8_t pitch,uint8_t trim,int8_t pan);
int ps_sem_add_drum(struct ps_sem *sem,int beatp,uint8_t drumid,uint8_t trim,int8_t pan);

/* Remove a note and all of its adjustments and sentinel.
 */
int ps_sem_remove_note(struct ps_sem *sem,int noteid);

/* Given a single event, remove it and its opposite if relevant.
 * If the event is NOTE or NOTEEND, remove all adjustments too.
 */
int ps_sem_remove_event(struct ps_sem *sem,int beatp,int eventp);

/* For drum events, do nothing.
 * For notes, change all pitches up to the nearest adjustments.
 */
int ps_sem_adjust_event_pitch(struct ps_sem *sem,int beatp,int eventp,int pitch);

/* Set (objid,trim,pan) on all events for a given note.
 * At the time of this writing, we do not plan to support trim or pan adjustment.
 * If that changes, this function might not make sense anymore.
 */
int ps_sem_modify_note(struct ps_sem *sem,int beatp,int eventp,int objid,int trim,int pan);

/* Pull an event from one beat and add it to another.
 * If the move breaks continuity, we fail but guarantee that the sem remain untouched.
 * On success, we return (eventp) in the new beat.
 * *** BEWARE *** In typical usage, errors should not be fatal.
 */
int ps_sem_move_event(struct ps_sem *sem,int beatp,int eventp,int dstbeatp);

/* Find a NOTE/NOTEEND pair, then move the NOTEEND to force the given length.
 * (beatc) is exclusive, ie it should never be less than one.
 */
int ps_sem_set_note_length(struct ps_sem *sem,int noteid,int beatc);

int ps_sem_find_note_by_noteid(int *beatp,int *eventp,const struct ps_sem *sem,int noteid);

/* How far can this event move?
 */
int ps_sem_get_event_movement_bounds(int *beatplo,int *beatphi,const struct ps_sem *sem,int beatp,int eventp);

/* If the command at (beatp,eventp) is a start or end, return coordinates of its other half.
 * If it's a singleton (ie DRUM or unknown), succeed and return the same coordinates.
 */
int ps_sem_find_partner(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp);

/* Find the NOTE command starting a chain, or the next event in a chain.
 * Finding next will fail at the end of the chain.
 */
int ps_sem_find_note_start(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp);
int ps_sem_find_note_next(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp);

/* For interaction with UI.
 * UI provides (beatp,pitch,position) and we set (*eventp) for the likeliest event the user clicked on.
 * (beatp) should be simple, from the horz position of the click.
 * (pitch) is either the 0..255 pitch row clicked on, or <0 if it's in the drum zone.
 * (position) is a qualifier of where the pointer was relative to the cell:
 *   0: left side of note, or bottom-left of drum.
 *   1: right side of note, or bottom-right of drum.
 *   2: top-left of drum.
 *   3: top-right of drum.
 * The (test_objid) callback lets you indicate whether an object is visible.
 */
int ps_sem_find_event_in_chart(
  int *eventp,
  const struct ps_sem *sem,
  int beatp,int pitch,int position,
  int (*test_objid)(uint8_t objid,void *userdata),
  void *userdata
);

/* Remove the song's commands and rebuild from sem.
 */
int ps_sem_write_song(struct akau_song *song,const struct ps_sem *sem);

/* Wipe the sem and rebuild it from the song.
 */
int ps_sem_read_song(struct ps_sem *sem,const struct akau_song *song);

/* For debugging only, summarize our content into the log.
 */
void ps_sem_dump(const struct ps_sem *sem);

/* Support for mass editing.
 *****************************************************************************/

/* Report the lowest and highest value for a given field among commands in the given range.
 * Your filter functions may return zero to ignore a given voice. If unset, we use all voices.
 */
int ps_sem_get_objid_in_range(
  uint8_t *drumidlo,uint8_t *drumidhi,uint8_t *instridlo,uint8_t *instridhi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_get_pitch_in_range(
  uint8_t *lo,uint8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_get_trim_in_range(
  uint8_t *lo,uint8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_get_pan_in_range(
  int8_t *lo,int8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);

/* Set one field in all matching commands in a given range.
 */
int ps_sem_set_objid_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t drumid,uint8_t instrid,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_set_pitch_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t pitch,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_set_trim_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t trim,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_set_pan_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int8_t pan,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);

/* Adjust one field in all matching commands in a given range.
 * It is safe to overflow; we clamp as necessary.
 * There is no 'objid' adjuster; I don't think that would make sense.
 */
int ps_sem_adjust_pitch_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_adjust_trim_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);
int ps_sem_adjust_pan_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
);

#endif
