#include "ps_input_internal.h"
#include "ps_input_device.h"
#include "ps_input_maptm.h"
#include "ps_input_map.h"
#include "ps_input_config.h"

/* Connect device.
 */
 
int ps_input_event_connect(struct ps_input_device *device) {
  if (!device) return -1;

  /* If we haven't loaded configuration yet, don't bother trying to map any device.
   * This is a little bit of a chicken-and-egg situation:
   *  - Configuration can't load without the providers installed, because it needs them for decoding.
   *  - Drivers want to create their provider and devices all at once when they initialize. That's reasonable.
   *  - So, the initial set of input devices are all installed before the configuration is loaded.
   * Global input manager is aware of this, and fixes it by reconnecting every unmapped device after a configuration load.
   */
  if (ps_input.preconfig) return 0;

  ps_log(INPUT,INFO,"Connecting input device '%.*s'.",device->namec,device->name);
  
  ps_input_map_del(device->map);
  device->map=0;

  struct ps_input_maptm *maptm=ps_input_config_select_maptm_for_device(ps_input.config,device);
  if (!maptm) {
    char providername[32];
    int providernamec=ps_input_provider_repr(providername,sizeof(providername),device->providerid);
    if (providernamec>(int)sizeof(providername)) providernamec=0;
    ps_log(INPUT,WARN,
      "No map template found for input device. provider=%.*s, name='%.*s', vendorid=0x%04x, deviceid=0x%04x",
      providernamec,providername,device->namec,device->name,device->vendorid,device->deviceid
    );
  }

  if (maptm) {
    if (!(device->map=ps_input_maptm_apply_for_device(maptm,device))) {
      ps_log(INPUT,ERROR,"Failed to apply map template to device '%.*s'.",device->namec,device->name);
      return -1;
    }
    ps_input.mapped_devices_changed=1;
  }
  
  return 0;
}

/* Disconnect device.
 */
 
int ps_input_event_disconnect(struct ps_input_device *device) {
  if (!device) return -1;

  ps_log(INPUT,INFO,"Disconnecting input device '%.*s'.",device->namec,device->name);

  if (device->map) {
    ps_input_map_del(device->map);
    device->map=0;
    ps_input.mapped_devices_changed=1;
  }
  
  return 0;
}

/* Device state changed.
 */

static int ps_input_event_button_cb(int plrid,int btnid,int value,void *userdata) {
  struct ps_input_device *device=userdata;
  ps_log(INPUT,TRACE,"%s plrid=%d btnid=%d value=%d device='%.*s'",__func__,plrid,btnid,value,device->namec,device->name);

  if (btnid&~0xffff) {
    if (value) {
      return ps_input_fire_action(btnid);
    }
  } else {
    if ((plrid>=1)&&(plrid<=PS_PLAYER_LIMIT)) {
      if (value) {
        ps_input.plrbtnv[plrid]|=btnid;
      } else {
        ps_input.plrbtnv[plrid]&=~btnid;
      }
    }
    if (value) {
      ps_input.plrbtnv[0]|=btnid;
    } else {
      ps_input.plrbtnv[0]&=~btnid;
    }
  }
  
  return 0;
}
 
int ps_input_event_button(struct ps_input_device *device,int btnid,int value) {
  if (!device) return -1;
  if (!device->map) return 0;
  if (ps_input_map_set_button(device->map,btnid,value,device,ps_input_event_button_cb)<0) return -1;
  return 0;
}
