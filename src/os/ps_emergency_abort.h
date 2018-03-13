/* ps_emergency_abort.h
 * My stopgap not-a-real-solution to a rare freeze at shutdown.
 * Spin up a background thread that aborts the process with an error message after a given interval.
 */
 
#ifndef PS_EMERGENCY_ABORT_H
#define PS_EMERGENCY_ABORT_H

int ps_emergency_abort_set(int timeout_us);
int ps_emergency_abort_cancel();
int ps_emergency_abort_get_time_remaining(); // <0 if unset

#endif
