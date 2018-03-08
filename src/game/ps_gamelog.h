/* ps_gamelog.h
 * Helper for tracking game events, probably only for test purposes.
 * First use case is to track blueprint usage.
 */

#ifndef PS_GAMELOG_H
#define PS_GAMELOG_H

struct ps_gamelog;

struct ps_gamelog *ps_gamelog_new();
void ps_gamelog_del(struct ps_gamelog *gamelog);
int ps_gamelog_ref(struct ps_gamelog *gamelog);

/* We commit log to disk periodically, let us worry about the details.
 * Call this every frame.
 */
int ps_gamelog_tick(struct ps_gamelog *gamelog);

int ps_gamelog_save(struct ps_gamelog *gamelog);
int ps_gamelog_load(struct ps_gamelog *gamelog);
int ps_gamelog_clear(struct ps_gamelog *gamelog);

int ps_gamelog_blueprint_used(struct ps_gamelog *gamelog,int blueprintid,int playerc);

#endif
