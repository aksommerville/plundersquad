/* ps_input.h
 */

#ifndef PS_INPUT_H
#define PS_INPUT_H

#define PS_INPUT_PROVIDER_MACHID      1
#define PS_INPUT_PROVIDER_EVDEV       2
#define PS_INPUT_PROVIDER_WINHID      3

int ps_input_init();
void ps_input_quit();

int ps_input_set_default_keyboard_mapping();
int ps_input_map_key(int keycode,int plrid,int dstbtnid);
int ps_input_map_jbtn(int provider,int devid,int srcbtnid,int lo,int hi,int plrid,int dstbtnid);

int ps_input_update();

/* Player buttons are the end result of input processing.
 * Buttons for a player may come from multiple devices.
 * plrid zero is a composite of all player inputs, including mapped but unassigned devices.
 * Users of this API shouldn't have to set or clear player buttons.
 */
uint16_t ps_get_player_buttons(int plrid);
int ps_get_player_button(int plrid,uint16_t btnid);
int ps_set_player_button(int plrid,uint16_t btnid,int value);
int ps_clear_player_buttons(int plrid);

/* Actions are stateless outputs for mapping.
 * Eg "quit" or "screenshot".
 * Action ID must be >0xffff, so they don't conflict with player buttons.
 * YOU implement ps_main_input_action_callback() -- we invoke it if there's no general action.
 * (That's a hack to enable a more context-aware level of the program to respond easily to actions).
 */
int ps_input_fire_action(int actionid);
int ps_main_input_action_callback(int actionid);

/* Providers should call these hooks to begin event processing.
 * The "keycode" for keyboard events should be USB-HID page 7.
 * Figure that out in the driver so we can all agree to a common language.
 * When reporting joystick events, you must give your own "provider id", in addition to a unique device id.
 */
int ps_input_rcvevt_focus(int focus);
int ps_input_rcvevt_key(int keycode,int codepoint,int value);
int ps_input_rcvevt_mmotion(int x,int y);
int ps_input_rcvevt_mbutton(int btnid,int value);
int ps_input_rcvevt_mwheel(int dx,int dy);
int ps_input_rcvevt_jconnect(int provider,int devid);
int ps_input_rcvevt_jdisconnect(int provider,int devid);
int ps_input_rcvevt_jbutton(int provider,int devid,int btnid,int value);

#endif
