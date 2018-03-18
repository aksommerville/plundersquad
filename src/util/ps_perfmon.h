/* ps_perfmon.h
 * Cheap performance monitor.
 */

#ifndef PS_PERFMON_H
#define PS_PERFMON_H

struct ps_perfmon;

struct ps_perfmon *ps_perfmon_new();
void ps_perfmon_del(struct ps_perfmon *perfmon);
int ps_perfmon_ref(struct ps_perfmon *perfmon);

/* If >0, call ps_perfmon_log() on myself during ps_perfmon_update().
 */
int ps_perfmon_set_autolog(struct ps_perfmon *perfmon,int64_t interval_us);

/* Application lifecycle events.
 * Construction and destruction of the monitor itself are implicit events too.
 */
int ps_perfmon_finish_load(struct ps_perfmon *perfmon);
int ps_perfmon_update(struct ps_perfmon *perfmon);
int ps_perfmon_begin_quit(struct ps_perfmon *perfmon);

/* Dump live status into the log, since the last call.
 */
int ps_perfmon_log(struct ps_perfmon *perfmon);

#endif
