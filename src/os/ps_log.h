/* ps_log.h
 * Our logging is based on "domains" and "levels".
 * This should feel familiar if you're used to log4j.
 * Each domain has its log level configured, and can be adjusted at runtime.
 * Each log write declares a domain, typically the source unit that generated the message.
 * Domains must be defined globally, right here.
 */

#ifndef PS_LOG_H
#define PS_LOG_H

#define PS_LOG_DOMAIN_                 0
#define PS_LOG_DOMAIN_MACIOC           1
#define PS_LOG_DOMAIN_CLOCK            2
#define PS_LOG_DOMAIN_RES              3
#define PS_LOG_DOMAIN_AUDIO            4
#define PS_LOG_DOMAIN_VIDEO            5
#define PS_LOG_DOMAIN_GENERATOR        6
#define PS_LOG_DOMAIN_INPUT            7
#define PS_LOG_DOMAIN_MAIN             8
#define PS_LOG_DOMAIN_EDIT             9
#define PS_LOG_DOMAIN_MACWM           10
#define PS_LOG_DOMAIN_TEST            11
#define PS_LOG_DOMAIN_SPRITE          12
#define PS_LOG_DOMAIN_GAME            13
#define PS_LOG_DOMAIN_TEXT            14
#define PS_LOG_DOMAIN_PHYSICS         15
#define PS_LOG_DOMAIN_MACHID          16
#define PS_LOG_DOMAIN_GUI             17
#define PS_LOG_DOMAIN_COUNT           18
// Feel free to add domains. Please update PS_LOG_DOMAIN_COUNT, and add default level in ps_log.c.

#define PS_LOG_LEVEL_ALL           0
#define PS_LOG_LEVEL_TRACE         1
#define PS_LOG_LEVEL_DEBUG         2
#define PS_LOG_LEVEL_INFO          3
#define PS_LOG_LEVEL_WARN          4
#define PS_LOG_LEVEL_WARNING       4
#define PS_LOG_LEVEL_ERROR         5
#define PS_LOG_LEVEL_FATAL         6
#define PS_LOG_LEVEL_SILENT        7

/* Use this function-like macro for normal logging.
 */
#define ps_log(domaintag,leveltag,fmt,...) ({ \
  if (ps_log_should_print(PS_LOG_DOMAIN_##domaintag,PS_LOG_LEVEL_##leveltag)) { \
    fprintf(ps_log_file?ps_log_file:stderr, \
      "%s" #domaintag "%s:%s" #leveltag "%s: " fmt " %s[%s:%d]%s\n", \
      ps_log_domain_color(PS_LOG_DOMAIN_##domaintag),ps_log_no_color(), \
      ps_log_level_color(PS_LOG_LEVEL_##leveltag),ps_log_no_color(), \
      ##__VA_ARGS__, \
      ps_log_faint_color(),__FILE__,__LINE__,ps_log_no_color() \
    ); \
    1; \
  } else { \
    0; \
  } \
})

/* By default, we log to stderr.
 * If ps_log_file is not NULL, we write there instead.
 */
extern FILE *ps_log_file;

/* By default (0), VT colors are enabled when ps_log_file is NULL.
 * Set >0 to always use color, or <0 never.
 */
extern int ps_log_use_color;

/* Current log level for each domain.
 * You can adjust this at runtime and it takes effect immediately.
 */
extern int ps_log_level_by_domain[PS_LOG_DOMAIN_COUNT];

/* VT color escape for each domain and level.
 * Valid for any input.
 */
const char *ps_log_domain_color(int domain);
const char *ps_log_level_color(int level);
const char *ps_log_no_color();
const char *ps_log_faint_color();

/* Nonzero if this domain/level is currently enabled.
 */
int ps_log_should_print(int domain,int level);

#endif
