#include "ps.h"
#include "ps_clockassist.h"
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#if PS_ARCH==PS_ARCH_mswin
  #include <Windows.h>
#endif

/* Every so many frames, we reconsider our mode. */
#define PS_CLOCKASSIST_CYCLE_LENGTH 5

/* Percentages, for mode-change thresholds. */
#define PS_CLOCKASSIST_MANUAL_ENGAGEMENT_THRESHOLD    50
#define PS_CLOCKASSIST_AUTOPILOT_ENGAGEMENT_THRESHOLD 20

/* Current time.
 */

int64_t ps_time_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Sleep.
 */

void ps_time_sleep(int us) {
  #if PS_ARCH==PS_ARCH_mswin
    Sleep(us/1000);
  #else
    usleep(us);
  #endif
}

/* Assistant clock, setup.
 */
 
int ps_clockassist_setup(struct ps_clockassist *clockassist,int rate_hz) {
  if (!clockassist) return -1;
  if (rate_hz<1) return -1;
  if (rate_hz>1000000) return -1;

  memset(clockassist,0,sizeof(struct ps_clockassist));
  clockassist->mode=PS_CLOCKASSIST_MODE_AUTO;
  clockassist->delay=1000000/rate_hz;
  clockassist->next=ps_time_now();
  
  clockassist->cyclep=0;
  clockassist->cycle_start_time=clockassist->next;
  clockassist->cycle_expect=clockassist->delay*PS_CLOCKASSIST_CYCLE_LENGTH;
  clockassist->manual_engagement_threshold=(clockassist->cycle_expect*PS_CLOCKASSIST_MANUAL_ENGAGEMENT_THRESHOLD)/100;
  clockassist->autopilot_engagement_threshold=(clockassist->cycle_expect*PS_CLOCKASSIST_AUTOPILOT_ENGAGEMENT_THRESHOLD)/100;
  clockassist->start=clockassist->next;

  ps_log(CLOCK,TRACE,"Clock starting in autopilot mode.");

  return 0;
}

/* Change mode.
 */

static int ps_clockassist_engage_manual(struct ps_clockassist *clock) {
  ps_log(CLOCK,TRACE,"Begin manual timing.");
  clock->mode=PS_CLOCKASSIST_MODE_MANUAL;
  clock->next=ps_time_now()+clock->delay;
  return 0;
}

static int ps_clockassist_engage_autopilot(struct ps_clockassist *clock) {
  ps_log(CLOCK,TRACE,"Begin autopilot timing.");
  clock->mode=PS_CLOCKASSIST_MODE_AUTO;
  return 0;
}

/* Reconsider mode.
 */

static int ps_clockassist_reconsider_mode(struct ps_clockassist *clock) {
  int64_t now=ps_time_now();
  int64_t elapsed=now-clock->cycle_start_time;
  if (elapsed<0) return -1;
  switch (clock->mode) {
    case PS_CLOCKASSIST_MODE_AUTO: {
        if (elapsed<clock->manual_engagement_threshold) {
          if (ps_clockassist_engage_manual(clock)<0) return -1;
        }
      } break;
    case PS_CLOCKASSIST_MODE_MANUAL: {
        if (clock->cycle_sleep_time<clock->autopilot_engagement_threshold) {
          if (ps_clockassist_engage_autopilot(clock)<0) return -1;
        }
      } break;
    default: return -1;
  }
  clock->cycle_start_time=now;
  clock->cyclep=0;
  clock->cycle_sleep_time=0;
  return 0;
}

/* Update.
 */

int ps_clockassist_update(struct ps_clockassist *clock) {
  if (!clock) return -1;

  clock->framec++;

  if (clock->cyclep++>=PS_CLOCKASSIST_CYCLE_LENGTH) {
    if (ps_clockassist_reconsider_mode(clock)<0) return -1;
  }

  switch (clock->mode) {
    case PS_CLOCKASSIST_MODE_AUTO: {
        clock->framec_autopilot++;
      } break;
    case PS_CLOCKASSIST_MODE_MANUAL: {
        clock->framec_manual++;
        int64_t now=ps_time_now();
        while (now<clock->next) {
          int sleep_request=clock->next-now;
          ps_time_sleep(sleep_request);
          int64_t next=ps_time_now();
          clock->cycle_sleep_time+=next-now;
          now=next;
        }
        clock->next+=clock->delay;
        if (clock->next<now) clock->next=now+clock->delay;
      } break;
    default: return -1;
  }
  
  return 0;
}
