/* ps_emergency_abort.h
 * My stopgap not-a-real-solution to a rare freeze at shutdown.
 * Spin up a background thread that aborts the process with an error message after a given interval.
 */
 
#ifndef PS_EMERGENCY_ABORT_H
#define PS_EMERGENCY_ABORT_H

int ps_emergency_abort_set(int timeout_us);
int ps_emergency_abort_cancel();
int ps_emergency_abort_get_time_remaining(); // <0 if unset

/* Log everywhere that there might be a stall at shutdown.
 * If we end up aborting, we'll dump the last provided message and how long since it was delivered.
 * *** Use string literals only! ***
 */
int ps_emergency_abort_set_message(const char *msg);

#endif
