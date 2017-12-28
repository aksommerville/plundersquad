/* ps_input_record.h
 * Structured playback of player input state.
 * This is used by assemblepage to animate the players.
 */

#ifndef PS_INPUT_RECORD_H
#define PS_INPUT_RECORD_H

struct ps_input_record_event {
  int delay;
  uint16_t state;
};

struct ps_input_record {
  int refc;
  struct ps_input_record_event *eventv;
  int eventc,eventa;
  int eventp;
  int delay;
  uint16_t state;
};

struct ps_input_record *ps_input_record_new();
void ps_input_record_del(struct ps_input_record *record);
int ps_input_record_ref(struct ps_input_record *record);

int ps_input_record_add_event(struct ps_input_record *record,int delay,uint16_t state);

int ps_input_record_restart(struct ps_input_record *record);

uint16_t ps_input_record_update(struct ps_input_record *record);

#endif
