#include "ps.h"

/* Current log level for each domain.
 */
 
int ps_log_level_by_domain[PS_LOG_DOMAIN_COUNT]={
#define _(domain,level) [PS_LOG_DOMAIN_##domain]=PS_LOG_LEVEL_##level,
  _(              ,TRACE)
  _(MACIOC        ,TRACE)
  _(CLOCK         ,INFO)
  _(RES           ,INFO)
  _(AUDIO         ,DEBUG)
  _(VIDEO         ,TRACE)
  _(GENERATOR     ,TRACE)
  _(INPUT         ,DEBUG)
  _(MAIN          ,TRACE)
  _(EDIT          ,TRACE)
  _(MACWM         ,TRACE)
  _(TEST          ,TRACE) /* TEST log level is also influenced by test verbosity. */
  _(SPRITE        ,TRACE)
  _(GAME          ,TRACE)
  _(TEXT          ,TRACE)
  _(PHYSICS       ,TRACE)
  _(MACHID        ,INFO)
  _(GUI           ,TRACE)
  _(HEROSTATE     ,DEBUG)
  _(EVDEV         ,INFO)
  _(GLX           ,TRACE)
  _(RESPACK       ,TRACE)
  _(MSWM          ,TRACE)
  _(MSAUDIO       ,TRACE)
  _(MSHID         ,TRACE)
  _(CONFIG        ,TRACE)
// Adding a log domain? Don't forget to add it in ps_enums.c
#undef _
};

/* Miscellaneous globals.
 */

FILE *ps_log_file=0;
int ps_log_use_color=0;
int ps_log_level_default=PS_LOG_LEVEL_ALL;

/* VT escapes.
 */

const char *ps_log_domain_color(int domain) {
  if (ps_log_use_color<0) return "";
  if (!ps_log_use_color&&ps_log_file) return "";
  switch (domain&7) {
    case 0: return "\x1b[31m";
    case 1: return "\x1b[32m";
    case 2: return "\x1b[33m";
    case 3: return "\x1b[34m";
    case 4: return "\x1b[35m";
    case 5: return "\x1b[36m";
    case 6: return "\x1b[37m";
    default:return "\x1b[38m";
  }
}

const char *ps_log_level_color(int level) {
  if (ps_log_use_color<0) return "";
  if (!ps_log_use_color&&ps_log_file) return "";
  switch (level) {
    case PS_LOG_LEVEL_TRACE: return "\x1b[40m";
    case PS_LOG_LEVEL_DEBUG: return "\x1b[44m";
    case PS_LOG_LEVEL_INFO:  return "\x1b[42;30m";
    case PS_LOG_LEVEL_WARN:  return "\x1b[43;30m";
    case PS_LOG_LEVEL_ERROR: return "\x1b[41m";
    case PS_LOG_LEVEL_FATAL: return "\x1b[45m";
  }
  return "";
}

const char *ps_log_no_color() {
  if (ps_log_use_color<0) return "";
  if (!ps_log_use_color&&ps_log_file) return "";
  return "\x1b[0m";
}

const char *ps_log_faint_color() {
  if (ps_log_use_color<0) return "";
  if (!ps_log_use_color&&ps_log_file) return "";
  return "\x1b[2m";
}

/* Nonzero if this domain/level is currently enabled.
 */
 
int ps_log_should_print(int domain,int level) {
  if ((domain<0)||(domain>=PS_LOG_DOMAIN_COUNT)) {
    return (level>=ps_log_level_default)?1:0;
  } else {
    return (level>=ps_log_level_by_domain[domain])?1:0;
  }
}

/* Print hex dump.
 */
 
void _ps_log_hexdump(FILE *f,const void *src,int srcc) {
  if (!f) f=stderr;
  const uint8_t *SRC=src;
  int srcp=0,i;
  while (srcp<srcc) {
    fprintf(f,"  %08x | ",srcp);
    for (i=0;i<16;i++) {
      if (srcp+i<srcc) {
        fprintf(f,"%02x ",SRC[srcp+i]);
      } else {
        fprintf(f,"   ");
      }
    }
    fprintf(f,"| ");
    for (i=0;i<16;i++) {
      if (srcp+i>=srcc) break;
      char ch=SRC[srcp+i];
      if ((ch>=0x20)&&(ch<=0x7e)) {
        fprintf(f,"%c",ch);
      } else {
        fprintf(f,".");
      }
    }
    fprintf(f,"\n");
    srcp+=16;
  }
}
