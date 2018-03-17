/* ps_clockassist.h
 * Helpers for timing.
 */

#ifndef PS_MACIOC_CLOCKASSIST_H
#define PS_MACIOC_CLOCKASSIST_H

#include <stdint.h>

int64_t ps_time_now();
void ps_time_sleep(int us);

#define PS_CLOCKASSIST_MODE_AUTO     1
#define PS_CLOCKASSIST_MODE_MANUAL   2

struct ps_clockassist {
  int mode;
  int64_t next;
  int delay;
  int cyclep;
  int64_t cycle_start_time;
  int64_t cycle_sleep_time;
  int64_t cycle_expect;
  int64_t manual_engagement_threshold;
  int64_t autopilot_engagement_threshold;
  int64_t framec_manual;
  int64_t framec_autopilot;
  int64_t framec;
  int64_t start;
};

int ps_clockassist_setup(struct ps_clockassist *clockassist,int rate_hz);

int ps_clockassist_update(struct ps_clockassist *clockassist);

#endif
