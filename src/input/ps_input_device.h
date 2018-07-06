/* ps_input_device.h
 * Generic representation of a source device.
 * This is part of the manager/driver interface.
 */

#ifndef PS_INPUT_DEVICE_H
#define PS_INPUT_DEVICE_H

struct ps_input_map;
struct ps_input_device;
struct ps_input_premap;
struct ps_input_report_reader;

struct ps_input_btncfg {

  int srcbtnid;
  int lo,hi;
  int value;
  
  // See aggregates in ps_input_button.h.
  // It's also safe to drop USB-HID usage in here, but that's not what it's for.
  int default_usage;
};

struct ps_input_btnwatch {
  int watchid;
  int (*cb)(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata);
  void (*userdata_del)(void *userdata);
  void *userdata;
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
  struct ps_input_premap *premap; // optional

  // Trigger your callback for each button on this device, until you return nonzero.
  int (*report_buttons)(
    struct ps_input_device *device,
    void *userdata,
    int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
  );

  struct ps_input_btnwatch *btnwatchv;
  int btnwatchc,btnwatcha;
  int btnwatchid_next;

  // Not used by the input manager; available here for providers' convenience.
  struct ps_input_report_reader *report_reader;
  
};

struct ps_input_device *ps_input_device_new(int size);
void ps_input_device_del(struct ps_input_device *device);
int ps_input_device_ref(struct ps_input_device *device);

int ps_input_device_set_name(struct ps_input_device *device,const char *src,int srcc);
int ps_input_device_set_name_utf16lez(struct ps_input_device *device,const char *src); // Convenience for USB support
int ps_input_device_set_map(struct ps_input_device *device,struct ps_input_map *map);
int ps_input_device_set_premap(struct ps_input_device *device,struct ps_input_premap *premap);

// Use some black magic to guess what kind of physical device this is.
int ps_input_device_is_joystick(const struct ps_input_device *device);
int ps_input_device_is_keyboard(const struct ps_input_device *device);

// Can it map to a player?
int ps_input_device_is_usable(const struct ps_input_device *device);

/* Watch activity on a device.
 * "mapped" events are those connected to a player, in our button namespace; this is usually what you want.
 * If not "mapped", we report buttons as the provider does, in some namespace probably meaningless to you.
 */
int ps_input_device_watch_buttons(
  struct ps_input_device *device,
  int (*cb)(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata),
  void (*userdata_del)(void *userdata),
  void *userdata
);
int ps_input_device_unwatch_buttons(struct ps_input_device *device,int watchid);

int ps_input_device_call_button_watchers(struct ps_input_device *device,int btnid,int value,int mapped);

/* Assign this to report_buttons if you are using report_reader, the rest is automatic.
 */
int ps_input_device_report_buttons_via_report_reader(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
);

#endif
