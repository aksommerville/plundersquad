#include "ps.h"
#include "ps_emergency_abort.h"
#include "ps_clockassist.h"
#include <unistd.h>

// In MinGW, <unistd.h> declares struct timespec but doesn't set this, then <pthread.h> tries to redefine it.
#define HAVE_STRUCT_TIMESPEC

#include <pthread.h>

/* Globals.
 */
 
static int64_t ps_emergency_abort_time=0;
static pthread_t ps_emergency_abort_thread; // No initializer; platforms disagree on its type.
static int ps_emergency_abort_init=0;
static const char *ps_emergency_abort_message=0;
static int64_t ps_emergency_abort_message_time=0;

/* Thread.
 */
 
static void *ps_emergency_abort_thdfn(void *arg) {
  while (1) {
    ps_time_sleep(1000);
    pthread_testcancel();
    if (ps_time_now()>=ps_emergency_abort_time) {
    
      ps_log(CLOCK,ERROR,"Shutdown may have stalled. Aborting hard.");
      
      if (ps_emergency_abort_message) {
        int64_t elapsed=ps_time_now()-ps_emergency_abort_message_time;
        ps_log(CLOCK,ERROR,
          "Last application activity was logged %d.%06d seconds ago:\n  %s\n",
          (int)(elapsed/1000000),(int)(elapsed%1000000),ps_emergency_abort_message
        );
      }
      
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
  if (!ps_emergency_abort_init) {
    ps_emergency_abort_init=1;
    pthread_create(&ps_emergency_abort_thread,0,ps_emergency_abort_thdfn,0);
  }
  return 0;
}

/* Cancel timer.
 */
 
int ps_emergency_abort_cancel() {
  if (ps_emergency_abort_init) {
    pthread_cancel(ps_emergency_abort_thread);
    pthread_join(ps_emergency_abort_thread,0);
    ps_emergency_abort_init=0;
  }
  ps_emergency_abort_time=0;
  ps_emergency_abort_message=0;
  ps_emergency_abort_message_time=0;
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

/* Set message for logging.
 */
 
int ps_emergency_abort_set_message(const char *msg) {
  if (!ps_emergency_abort_time) return -1;
  ps_emergency_abort_message=msg;
  ps_emergency_abort_message_time=ps_time_now();
  return 0;
}
