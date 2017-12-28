#include "ps.h"
#include "ps_input_record.h"

/* Object lifecycle.
 */
 
struct ps_input_record *ps_input_record_new() {
  struct ps_input_record *record=calloc(1,sizeof(struct ps_input_record));
  if (!record) return 0;

  record->refc=1;

  return record;
}

void ps_input_record_del(struct ps_input_record *record) {
  if (!record) return;
  if (record->refc-->1) return;

  if (record->eventv) free(record->eventv);

  free(record);
}

int ps_input_record_ref(struct ps_input_record *record) {
  if (!record) return -1;
  if (record->refc<1) return -1;
  if (record->refc==INT_MAX) return -1;
  record->refc++;
  return 0;
}

/* Add event.
 */

static int ps_input_record_require(struct ps_input_record *record) {
  if (record->eventc<record->eventa) return 0;
  int na=record->eventa+16;
  if (na>INT_MAX/sizeof(struct ps_input_record_event)) return -1;
  void *nv=realloc(record->eventv,sizeof(struct ps_input_record_event)*na);
  if (!nv) return -1;
  record->eventv=nv;
  record->eventa=na;
  return 0;
}

int ps_input_record_add_event(struct ps_input_record *record,int delay,uint16_t state) {
  if (!record) return -1;
  if (delay<0) return -1;
  if (ps_input_record_require(record)<0) return -1;
  struct ps_input_record_event *event=record->eventv+record->eventc++;
  event->delay=delay;
  event->state=state;
  return 0;
}

/* Restart playback.
 */

int ps_input_record_restart(struct ps_input_record *record) {
  if (!record) return -1;
  record->eventp=0;
  record->delay=0;
  record->state=0;
  return 0;
}

/* Update.
 */

uint16_t ps_input_record_update(struct ps_input_record *record) {
  if (!record) return 0;
  if (record->eventc<1) return 0;
  if (record->delay>0) {
    record->delay--;
  } else {
    if (record->eventp>=record->eventc) record->eventp=0;
    record->state=record->eventv[record->eventp].state;
    record->delay=record->eventv[record->eventp].delay;
    record->eventp++;
  }
  return record->state;
}
