#ifndef AKAU_GLOBAL_INTERNAL_H
#define AKAU_GLOBAL_INTERNAL_H

#include <string.h>
#include "../akau.h"

struct akau_syncwatcher {
  int id;
  int (*cb)(uint16_t token,void *userdata);
  void (*userdata_del)(void *userdata);
  void *userdata;
};

extern struct akau {
  int init;
  struct akau_driver driver;
  void (*cb_log)(int loglevel,const char *msg,int msgc);
  int error;
  int rate;
  
  struct akau_mixer *mixer;
  struct akau_store *store;

  struct akau_syncwatcher *syncwatcherv;
  int syncwatcherc,syncwatchera;

  uint16_t *tokenv;
  int tokenc,tokena;

  int (*cb_clip)(int l,int r);
  int cliplc,cliprc;

  void (*cb_observer)(const int16_t *v,int c,void *userdata);
  void *userdata_observer;
  
} akau;

#endif
