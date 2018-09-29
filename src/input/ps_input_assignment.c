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

/* Drop all device assignments.
 */
 
int ps_input_drop_device_assignments() {
  int provideri=ps_input.providerc;
  while (provideri-->0) {
    struct ps_input_provider *provider=ps_input.providerv[provideri];
    int devicei=provider->devc;
    while (devicei-->0) {
      struct ps_input_device *device=provider->devv[devicei];
      if (device->map) {
        device->map->plrid=0;
      }
    }
  }
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

/* This is for device connection during play.
 * If the game sees a device connection while running, assign it to the player with the fewest devices.
 * In normal cases where player and device are one-to-one, a device can disconnect and reconnect and things are restored normally.
 */
 
int ps_input_assign_device_to_least_covered_player(struct ps_input_device *device,int playerc) {
  if (!device) return -1;
  if (!device->map) return -1;
  if ((playerc<1)||(playerc>PS_PLAYER_LIMIT)) return -1;

  int devc_by_playerid[1+PS_PLAYER_LIMIT]={0};
  int providerp=0; for (;providerp<ps_input.providerc;providerp++) {
    struct ps_input_provider *provider=ps_input.providerv[providerp];
    int devicep=0; for (;devicep<provider->devc;devicep++) {
      struct ps_input_device *pdevice=provider->devv[devicep];
      if (!pdevice->map) continue;
      if ((pdevice->map->plrid<1)||(pdevice->map->plrid>PS_PLAYER_LIMIT)) continue;
      devc_by_playerid[pdevice->map->plrid]++;
    }
  }

  int devc_min=INT_MAX;
  int playerid=1;
  int i=1; for (;i<playerc;i++) {
    if (!devc_by_playerid[i]) {
      playerid=i;
      break;
    }
    if (devc_by_playerid[i]<devc_min) {
      playerid=i;
      devc_min=devc_by_playerid[i];
    }
  }
  ps_log(INPUT,INFO,"Selected player id %d for new device '%s'.",playerid,device->name);
  device->map->plrid=playerid;
  
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
