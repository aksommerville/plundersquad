#include "ps.h"
#include "ps_perfmon.h"
#include "os/ps_clockassist.h"

/* Object.
 */

struct ps_perfmon {
  int refc;
  
  int64_t t_startup;
  int64_t t_load;
  int64_t t_finish;
  int64_t t_shutdown;

  int64_t framec;

  int64_t t_recent;
  int64_t framec_recent;

  int64_t autolog;
};

/* Reports.
 */

static void ps_perfmon_report_load(const struct ps_perfmon *perfmon) {
  int64_t elapsed=perfmon->t_load-perfmon->t_startup;
  ps_log(CLOCK,INFO,"Startup in %d.%06d s",(int)(elapsed/1000000),(int)(elapsed%1000000));
}

static void ps_perfmon_report_recent(const struct ps_perfmon *perfmon,int64_t now) {
  if (perfmon->framec_recent>0) {
    int64_t elapsed=now-perfmon->t_recent;
    if (elapsed>0) {
      double rate=(perfmon->framec_recent*1000000.0)/elapsed;
      ps_log(CLOCK,DEBUG,
        "%lld frames in %d.%06d s: %.02f Hz",
        perfmon->framec_recent,(int)(elapsed/1000000),(int)(elapsed%1000000),rate
      );
    }
  }
}

static void ps_perfmon_report_game(const struct ps_perfmon *perfmon) {
  if (perfmon->framec>0) {
    int64_t elapsed=perfmon->t_finish-perfmon->t_load;
    if (elapsed>0) {
      double rate=(perfmon->framec*1000000.0)/elapsed;
      ps_log(CLOCK,INFO,
        "Average: %lld frames in %d.%06d s: %.02f Hz",
        perfmon->framec,(int)(elapsed/1000000),(int)(elapsed%1000000),rate
      );
    }
  }
}

static void ps_perfmon_report_final(const struct ps_perfmon *perfmon) {
  int64_t elapsed=perfmon->t_shutdown-perfmon->t_finish;
  ps_log(CLOCK,INFO,"Shutdown in %d.%06d s",(int)(elapsed/1000000),(int)(elapsed%1000000));
}

/* Object lifecycle.
 */
 
struct ps_perfmon *ps_perfmon_new() {
  struct ps_perfmon *perfmon=calloc(1,sizeof(struct ps_perfmon));
  if (!perfmon) return 0;

  perfmon->refc=1;
  perfmon->t_startup=ps_time_now();

  return perfmon;
}

void ps_perfmon_del(struct ps_perfmon *perfmon) {
  if (!perfmon) return;
  if (perfmon->refc-->1) return;

  perfmon->t_shutdown=ps_time_now();
  ps_perfmon_report_final(perfmon);

  free(perfmon);
}

int ps_perfmon_ref(struct ps_perfmon *perfmon) {
  if (!perfmon) return -1;
  if (perfmon->refc<1) return -1;
  if (perfmon->refc==INT_MAX) return -1;
  perfmon->refc++;
  return 0;
}

/* Accessors.
 */
 
int ps_perfmon_set_autolog(struct ps_perfmon *perfmon,int64_t interval_us) {
  if (!perfmon) return -1;
  perfmon->autolog=interval_us;
  return 0;
}

/* Application lifecycle events.
 */
 
int ps_perfmon_finish_load(struct ps_perfmon *perfmon) {
  if (!perfmon) return -1;
  perfmon->t_load=ps_time_now();
  perfmon->t_recent=perfmon->t_load;
  perfmon->framec=0;
  perfmon->framec_recent=0;
  ps_perfmon_report_load(perfmon);
  return 0;
}

int ps_perfmon_update(struct ps_perfmon *perfmon) {
  if (!perfmon) return -1;
  
  perfmon->framec++;
  perfmon->framec_recent++;

  if (perfmon->autolog>0) {
    int64_t elapsed=ps_time_now()-perfmon->t_recent;
    if (elapsed>=perfmon->autolog) {
      ps_perfmon_log(perfmon);
    }
  }
  
  return 0;
}

int ps_perfmon_begin_quit(struct ps_perfmon *perfmon) {
  if (!perfmon) return 0;
  perfmon->t_finish=ps_time_now();
  ps_perfmon_report_game(perfmon);
  return 0;
}

/* Dump live status into the log, since the last call.
 */
 
int ps_perfmon_log(struct ps_perfmon *perfmon) {
  if (!perfmon) return -1;
  int64_t now=ps_time_now();
  ps_perfmon_report_recent(perfmon,now);
  perfmon->t_recent=now;
  perfmon->framec_recent=0;
  return 0;
}
