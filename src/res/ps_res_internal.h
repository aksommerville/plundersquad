#ifndef PS_RES_INTERNAL_H
#define PS_RES_INTERNAL_H

#include "ps.h"
#include "ps_resmgr.h"
#include "ps_restype.h"

extern struct ps_resmgr {
  int init;
  int edit;

  char *rootpath;
  int rootpathc;

  struct ps_restype typev[PS_RESTYPE_COUNT];

} ps_resmgr;

#endif
