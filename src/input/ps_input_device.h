/* ps_input_device.h
 * Generic representation of a source device.
 * This is part of the manager/driver interface.
 */

#ifndef PS_INPUT_DEVICE_H
#define PS_INPUT_DEVICE_H

struct ps_input_map;

struct ps_input_btncfg {
  int srcbtnid;
  int lo,hi;
  int value;
  int default_usage;
};

struct ps_input_device {
  int refc;
  
  int providerid;
  int devid;
  char *name;
  int namec;
  uint16_t vendorid; // eg from USB
  uint16_t deviceid; // eg from USB
  
  struct ps_input_map *map; // NULL if detached from manager.

  // Trigger your callback for each button on this device, until you return nonzero.
  int (*report_buttons)(
    struct ps_input_device *device,
    void *userdata,
    int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
  );
  
};

struct ps_input_device *ps_input_device_new(int size);
void ps_input_device_del(struct ps_input_device *device);
int ps_input_device_ref(struct ps_input_device *device);

int ps_input_device_set_name(struct ps_input_device *device,const char *src,int srcc);
int ps_input_device_set_map(struct ps_input_device *device,struct ps_input_map *map);

#endif
