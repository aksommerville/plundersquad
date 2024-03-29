/* ps_input.h
 * Global input manager.
 *
 * Bootstrap sequence:
 *   - ps_input_init()
 *   - ps_input_install_provider() for each driver
 *   - ps_input_load_configuration()
 * It is technically possible to add providers after configuring, but their map templates will not be loaded.
 *
 */

#ifndef PS_INPUT_H
#define PS_INPUT_H

#include "ps_input_button.h"

struct ps_input_provider;
struct ps_input_device;
struct ps_input_config;
struct ps_gui;

/* Input manager global API.
 *****************************************************************************/

int ps_input_init();
void ps_input_quit();

/* Provider list.
 */
int ps_input_install_provider(struct ps_input_provider *provider);
int ps_input_uninstall_provider(struct ps_input_provider *provider);
int ps_input_count_providers();
struct ps_input_provider *ps_input_get_provider_by_index(int index);
struct ps_input_provider *ps_input_get_provider_by_id(int providerid);

int ps_input_set_gui(struct ps_gui *gui);

int ps_input_load_configuration(const char *path);
struct ps_input_config *ps_input_get_configuration();

int ps_input_update();

int ps_input_set_player_count(int playerc);
int ps_input_force_device_assignment(struct ps_input_device *device,int playerid);
int ps_input_set_noninteractive_device_assignment(); // Force device assignments willy-nilly, for test launches.
int ps_input_assign_device_to_least_covered_player(struct ps_input_device *device,int playerc);
int ps_input_drop_device_assignments();

int ps_input_request_termination();
int ps_input_termination_requested();

/* Register your interest to receive device connect and disconnect events.
 * When you register, we immediately fire callbacks for all existing devices.
 * Watching returns a 'watchid' which you must use later to unwatch.
 */
int ps_input_watch_devices(
  int (*cb_connect)(struct ps_input_device *device,void *userdata),
  int (*cb_disconnect)(struct ps_input_device *device,void *userdata),
  void *userdata
);
int ps_input_unwatch_devices(int watchid);

/* State for the public to read.
 *****************************************************************************/

uint16_t ps_get_player_buttons(int plrid);

/* Force all A, B, and START buttons to read zero for so many frames.
 * The internal state is not affected; we apply this filter in ps_get_player_buttons().
 */
int ps_input_suppress_player_actions(int duration);

/* Toggle device assignments so we can control multiple heroes with one input device.
 * Also used by ps_sprite_lwizard (it's not just for debugging).
 */
int ps_input_swap_assignments();

/* Events for providers to trigger.
 *****************************************************************************/

/* Create a device and install it inside the provider, then fire the 'connect' event.
 * On disconnection, uninstallation is up to you; we don't care about the sequence.
 */
int ps_input_event_connect(struct ps_input_device *device);
int ps_input_event_disconnect(struct ps_input_device *device);

/* Report state of a button on some physical device.
 */
int ps_input_event_button(struct ps_input_device *device,int btnid,int value);

/* Manager will call this internally, but you can too.
 */
int ps_input_fire_action(int actionid);

/* System pointer device.
 */
int ps_input_event_mmotion(int x,int y);
int ps_input_event_mbutton(int btnid,int value);
int ps_input_event_mwheel(int dx,int dy);

/* System keyboard.
 * (keycode) is USB-HID, because it's something all platforms can agree on.
 * (codepoint) is Unicode.
 */
int ps_input_event_key(int keycode,int codepoint,int value);

#endif
