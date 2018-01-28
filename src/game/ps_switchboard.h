/* ps_switchboard.h
 * Manages a set of switches, reset at screen load.
 * Switches can be actuated by footswitches, buttons, etc.
 * Their output is a grid barrier or certain sprites (eg killozap).
 * Each actuator and each output connects through the switchboard, 
 * so there is one consistent state per switch.
 * Singleton owned by ps_game.
 */

#ifndef PS_SWITCHBOARD_H
#define PS_SWITCHBOARD_H

struct ps_switchboard;

struct ps_switchboard *ps_switchboard_new();
void ps_switchboard_del(struct ps_switchboard *switchboard);
int ps_switchboard_ref(struct ps_switchboard *switchboard);

/* Switchboard has only one callback and it doesn't retain the userdata.
 * This is called any time a switch changes.
 */
int ps_switchboard_set_callback(
  struct ps_switchboard *switchboard,
  int (*cb)(int switchid,int value,void *userdata),
  void *userdata
);

/* Remove all switch definitions, eg at screen load.
 * Your call if you want to be called for anything that changes here.
 */
int ps_switchboard_clear(struct ps_switchboard *switchboard,int fire_callbacks);

/* Define a switch by setting it.
 * (switchid) must be >0. They do not need to be contiguous.
 * It's legal to read undefined switches; they have an implicit value of zero.
 */
int ps_switchboard_set_switch(struct ps_switchboard *switchboard,int switchid,int value);
int ps_switchboard_get_switch(const struct ps_switchboard *switchboard,int switchid);

/* Sequential access.
 */
int ps_switchboard_count_switches(const struct ps_switchboard *switchboard);
int ps_switchboard_get_switch_by_index(int *switchid,const struct ps_switchboard *switchboard,int index);

#endif
