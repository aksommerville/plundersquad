#ifndef PS_INPUT_INTERNAL_H
#define PS_INPUT_INTERNAL_H

#include "ps.h"
#include "ps_input.h"

struct ps_gui;

struct ps_input_watch {
  int watchid;
  int (*cb_connect)(struct ps_input_device *device,void *userdata);
  int (*cb_disconnect)(struct ps_input_device *device,void *userdata);
  void *userdata;
};

/* Global state.
 *****************************************************************************/

extern struct ps_input {
  int init;

  struct ps_input_provider **providerv;
  int providerc,providera;

  struct ps_input_config *config;

  int preconfig; // If set, don't bother trying to connect devices. They'll try again after configuration loads.

  uint16_t plrbtnv[1+PS_PLAYER_LIMIT];
  int playerc;
  int suppress_player_buttons;

  int termination_requested;

  struct ps_input_watch *watchv;
  int watchc,watcha;

  struct ps_gui *gui;
  
} ps_input;

/* Private API.
 *****************************************************************************/

#endif
