#include "ps_input_internal.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"
#include "ps_input_map.h"

/* This file is suspiciously small because it used to handle the dynamic device-to-player assignment.
 * Now it's all interactive, so we can be nice and dumb about it.
 */

/* Set player count, public.
 */
 
int ps_input_set_player_count(int playerc) {
  if ((playerc<0)||(playerc>PS_PLAYER_LIMIT)) return -1;
  ps_input.playerc=playerc;
  return 0;
}

/* Manual assignment for a single device.
 * Initial player configuration happens at the device level, before assignment.
 * When player selection is committed to the game, we have very conveniently each player and his device.
 */
 
int ps_input_force_device_assignment(struct ps_input_device *device,int playerid) {
  if (!device) return -1;
  if (!device->map) return -1;
  if ((playerid<0)||(playerid>PS_PLAYER_LIMIT)) playerid=0;
  device->map->plrid=playerid;
  memset(ps_input.plrbtnv,0,sizeof(ps_input.plrbtnv));
  return 0;
}

/* Set all device assignments willy-nilly.
 * This is only needed if you are skipping the automatic input config.
 */

int ps_input_set_noninteractive_device_assignment() {

  ps_log(INPUT,WARN,"Overwriting device assignments.");

  int playerp=1;
  int providerp=0;
  for (;providerp<ps_input.providerc;providerp++) {
    struct ps_input_provider *provider=ps_input.providerv[providerp];
    int devicep=0; for (;devicep<provider->devc;devicep++) {
      struct ps_input_device *device=provider->devv[devicep];
      if (!ps_input_device_is_usable(device)) continue;

      ps_log(INPUT,INFO,"  %.*s => %d",device->namec,device->name,playerp);
      device->map->plrid=playerp;
      if (++playerp>ps_input.playerc) playerp=1;

    }
  }

  return 0;
}

/* Swap assignments for debugging.
 */

int ps_input_swap_assignments() {
  if (ps_input.playerc<1) return 0;
  memset(ps_input.plrbtnv,0,sizeof(ps_input.plrbtnv));
  int providerp=0; for (;providerp<ps_input.providerc;providerp++) {
    struct ps_input_provider *provider=ps_input.providerv[providerp];
    int devicep=0; for (;devicep<provider->devc;devicep++) {
      struct ps_input_device *device=provider->devv[devicep];
      if (!device->map) continue;
      if (++(device->map->plrid)>ps_input.playerc) device->map->plrid=1;
      ps_input.plrbtnv[device->map->plrid]|=PS_PLRBTN_CD;
    }
  }
  return 0;
}
