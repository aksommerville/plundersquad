/* ps_evdev.h
 * Interface to Linux event devices.
 */

#ifndef PS_EVDEV_H
#define PS_EVDEV_H

/* Main global interface.
 */

/* Initialize evdev and register with ps_input.
 */
int ps_evdev_init_default();

/* cb_disconnect decides whether errors fall back through ps_evdev_update().
 * If (reason<0) a real error occurred, or one of your other callbacks failed.
 * If (!reason) the device closed normally, eg user unplugged it.
 * If (!devid), the failure was in our inotify watch rather than a device.
 * cb_disconnect is *not* called for devices that remain connected at quit.
 * cb_connect may be called during initialization (in fact it almost certainly will).
 * Events are processed only slightly before triggering cb_event:
 *   - EV_SYN is ignored altogether.
 *   - EV_KEY are recorded in the device. Redundant EV_KEY are reported as received.
 *   - EV_ABS are clamped to the declared range and redundant reports are discarded.
 */
int ps_evdev_init(
  const char *path,
  int (*cb_connect)(int devid),
  int (*cb_disconnect)(int devid,int reason),
  int (*cb_event)(int devid,int type,int code,int value)
);

void ps_evdev_quit();

int ps_evdev_update();

/* Device properties.
 */

/* Index runs from zero through (count-1).
 * Devid are assigned sequentially starting with one.
 * Once opened, a devid will not be reused.
 * (When they reach INT_MAX, no more devices can be opened. This will never happen realistically.)
 * Evid are assigned by the kernel, eg "/dev/input/event3" is evid 3.
 */
int ps_evdev_count_devices();
int ps_evdev_devid_for_index(int index);
int ps_evdev_devid_for_evid(int evid);

/* Name, IDs, event bits, and axis ranges are retrieved at connect.
 * Getting them here does not require any I/O.
 * We record the state of ABS and KEY events.
 */
const char *ps_evdev_get_name(int devid);
int ps_evdev_get_id(int *bustype,int *vendor,int *product,int *version,int devid); // => evid
int ps_evdev_get_abs(int *lo,int *hi,int devid,int code); // => value
int ps_evdev_get_key(int devid,int code); // => value
int ps_evdev_has_key(int devid,int code); // => presence
int ps_evdev_has_rel(int devid,int code); // => presence

/* Hit your callback for every KEY, ABS, and REL event reported by this device.
 * Stop if (cb) returns nonzero, returning the same.
 */
int ps_evdev_report_capabilities(int devid,int (*cb)(int devid,int type,int code,int lo,int hi,int value,void *userdata),void *userdata);

/* Return our best guess at a USB-HID usage code for this event, or zero.
 */
int ps_evdev_default_usage_for_event(int type,int code);
int ps_evdev_translate_key_code(int *keycode,int *codepoint,int type,int code);

#endif
