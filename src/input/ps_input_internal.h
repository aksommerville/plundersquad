#ifndef PS_INPUT_INTERNAL_H
#define PS_INPUT_INTERNAL_H

#include "ps.h"
#include "ps_input.h"

/* Global state.
 *****************************************************************************/

extern struct ps_input {
  int init;

  struct ps_input_provider **providerv;
  int providerc,providera;

  struct ps_input_config *config;

  int mapped_devices_changed; // If set, we reconsider player assignments at the end of the cycle.
  int preconfig; // If set, don't bother trying to connect devices. They'll try again after configuration loads.

  uint16_t plrbtnv[1+PS_PLAYER_LIMIT];
  int playerc;

  int termination_requested;
  
} ps_input;

/* Private API.
 *****************************************************************************/

#endif
