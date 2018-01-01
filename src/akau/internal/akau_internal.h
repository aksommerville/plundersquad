#ifndef AKAU_INTERNAL_H
#define AKAU_INTERNAL_H

#include "ps_config.h"

/* Byte order. */
#if PS_ARCH==PS_ARCH_macos
  #include <machine/endian.h>
#elif PS_ARCH==PS_ARCH_linux
  #include <endian.h>
#elif PS_ARCH==PS_ARCH_raspi
  #include <endian.h>
#elif PS_ARCH==PS_ARCH_mswin
  /* MinGW doesn't seem to have endian.h, but I think Windows only runs on little-endian systems. */
  #define BIG_ENDIAN    4321
  #define LITTLE_ENDIAN 1234
  #define BYTE_ORDER LITTLE_ENDIAN
#else
  #error "Unable to guess byte order."
#endif

int akau_error(const char *fmt,...);
int akau_warn(const char *fmt,...);
int akau_info(const char *fmt,...);
int akau_debug(const char *fmt,...);

int akau_get_master_rate();

#endif
