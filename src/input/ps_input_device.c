#include "ps.h"
#include "ps_input_device.h"
#include "ps_input_map.h"

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
