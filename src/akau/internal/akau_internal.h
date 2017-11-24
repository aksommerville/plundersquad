#ifndef AKAU_INTERNAL_H
#define AKAU_INTERNAL_H

/* Byte order -- platform-specific. (TODO) */
#if __APPLE__
  #include <machine/endian.h>
#else
  #error "Unable to detect host byte order."
#endif

int akau_error(const char *fmt,...);
int akau_warn(const char *fmt,...);
int akau_info(const char *fmt,...);
int akau_debug(const char *fmt,...);

int akau_get_master_rate();

#endif
