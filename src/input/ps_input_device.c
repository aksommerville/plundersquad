#include "ps.h"
#include "ps_input_device.h"
#include "ps_input_map.h"
#include "ps_input_button.h"

/* Object lifecycle.
 */

struct ps_input_device *ps_input_device_new(int size) {
  if (!size) size=sizeof(struct ps_input_device);
  else if (size<(int)sizeof(struct ps_input_device)) return 0;

  struct ps_input_device *device=calloc(1,size);
  if (!device) return 0;

  device->refc=1;

  return device;
}

void ps_input_device_del(struct ps_input_device *device) {
  if (!device) return;
  if (device->refc-->1) return;

  if (device->name) free(device->name);
  if (device->map) ps_input_map_del(device->map);

  free(device);
}

int ps_input_device_ref(struct ps_input_device *device) {
  if (!device) return -1;
  if (device->refc<1) return -1;
  if (device->refc==INT_MAX) return -1;
  device->refc++;
  return 0;
}

/* Trivial accessors.
 */

int ps_input_device_set_name(struct ps_input_device *device,const char *src,int srcc) {
  if (!device) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>1024) return -1;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=srcc;
  return 0;
}

int ps_input_device_set_map(struct ps_input_device *device,struct ps_input_map *map) {
  if (!device) return -1;
  if (device->map==map) return 0;
  if (map&&(ps_input_map_ref(map)<0)) return -1;
  ps_input_map_del(device->map);
  device->map=map;
  return 0;
}

/* What kind of device?
 */

static int ps_input_device_has_maps_in_0007xxxx(const struct ps_input_device *device) {
  if (!device) return 0;
  if (!device->map) return 0;
  const struct ps_input_map_fld *fld=device->map->fldv;
  int i=device->map->fldc;
  for (;i-->0;fld++) {
    if ((fld->srcbtnid>=0x00070000)&&(fld->srcbtnid<=0x0007ffff)) return 1;
  }
  return 0;
}
 
int ps_input_device_is_joystick(const struct ps_input_device *device) {
  if (!device) return 0;
  switch (device->providerid) {
    // Lots of providers will never give us a joystick.
    case PS_INPUT_PROVIDER_macioc:
    case PS_INPUT_PROVIDER_macwm:
    case PS_INPUT_PROVIDER_x11:
    case PS_INPUT_PROVIDER_mswm:
      return 0;
  }
  if (!device->map) return 0;
  return ps_input_device_has_maps_in_0007xxxx(device)?0:1;
}

int ps_input_device_is_keyboard(const struct ps_input_device *device) {
  if (!device) return 0;
  switch (device->providerid) {
    case PS_INPUT_PROVIDER_macioc: return 0;
    case PS_INPUT_PROVIDER_macwm: return device->map?1:0;
    case PS_INPUT_PROVIDER_machid: return 0;
    case PS_INPUT_PROVIDER_evdev:
      #if PS_ARCH==PS_ARCH_x11
        return 0;
      #elif PS_ARCH==PS_ARCH_raspi
        return ps_input_device_has_maps_in_0007xxxx(device);
      #else
        return 0;
      #endif
    case PS_INPUT_PROVIDER_x11: return device->map?1:0;
    case PS_INPUT_PROVIDER_mswm: return device->map?1:0;
    case PS_INPUT_PROVIDER_mshid: return 0;
  }
  return 0;
}
