#include "ps.h"
#include "ps_emergency_abort.h"
#include "ps_clockassist.h"
#include <unistd.h>
#include <pthread.h>

/* Globals.
 */
 
static int64_t ps_emergency_abort_time=0;
static pthread_t ps_emergency_abort_thread=0;

/* Thread.
 */
 
static void *ps_emergency_abort_thdfn(void *arg) {
  while (1) {
    usleep(1000);
    pthread_testcancel();
    if (ps_time_now()>=ps_emergency_abort_time) {
      ps_log(CLOCK,ERROR,"Shutdown may have stalled. Aborting hard.");
      exit(1);
    }
  }
  return 0;
}

/* Set timer.
 */
 
int ps_emergency_abort_set(int timeout_us) {
  ps_log(CLOCK,INFO,"Set emergency abort timeout for %d us.",timeout_us);
  ps_emergency_abort_time=ps_time_now()+timeout_us;
  if (!ps_emergency_abort_thread) {
    pthread_create(&ps_emergency_abort_thread,0,ps_emergency_abort_thdfn,0);
  }
  return 0;
}

/* Cancel timer.
 */
 
int ps_emergency_abort_cancel() {
  if (ps_emergency_abort_thread) {
    pthread_cancel(ps_emergency_abort_thread);
    pthread_join(ps_emergency_abort_thread,0);
    ps_emergency_abort_thread=0;
  }
  ps_emergency_abort_time=0;
  return 0;
}

/* Query time.
 */
 
int ps_emergency_abort_get_time_remaining() {
  if (!ps_emergency_abort_time) return -1;
  int64_t remaining=ps_emergency_abort_time-ps_time_now();
  if (remaining<0) return 0;
  if (remaining>INT_MAX) return INT_MAX;
  return remaining;
}
