/* ps_input_button.h
 * Symbols for input.
 */

#ifndef PS_INPUT_BUTTON_H
#define PS_INPUT_BUTTON_H

#define PS_PLRBTN_UP     0x0001
#define PS_PLRBTN_DOWN   0x0002
#define PS_PLRBTN_LEFT   0x0004
#define PS_PLRBTN_RIGHT  0x0008
#define PS_PLRBTN_A      0x0010
#define PS_PLRBTN_B      0x0020
#define PS_PLRBTN_START  0x0040
#define PS_PLRBTN_CD     0x8000

#define PS_PLRBTN_ALWAYS_AVAILABLE 0x800f /* Immune to player button suppression. */
#define PS_PLRBTN_MAPPABLE 0x007f /* Buttons that should be mapped. */

// These aggregate markers are only for ps_input_btncfg.default_usage
#define PS_PLRBTN_HORZ   (PS_PLRBTN_LEFT|PS_PLRBTN_RIGHT)
#define PS_PLRBTN_VERT   (PS_PLRBTN_UP|PS_PLRBTN_DOWN)
#define PS_PLRBTN_THUMB  (PS_PLRBTN_A|PS_PLRBTN_B)
#define PS_PLRBTN_AUX    (PS_PLRBTN_START)

#define PS_MBTN_LEFT    1
#define PS_MBTN_MIDDLE  2
#define PS_MBTN_RIGHT   3
#define PS_MBTN_AUX     4

#define PS_INPUT_PROVIDER_UNSET        0
#define PS_INPUT_PROVIDER_macioc       1
#define PS_INPUT_PROVIDER_macwm        2
#define PS_INPUT_PROVIDER_machid       3
#define PS_INPUT_PROVIDER_evdev        4
#define PS_INPUT_PROVIDER_x11          5
#define PS_INPUT_PROVIDER_mswm         6
#define PS_INPUT_PROVIDER_mshid        7
#define PS_INPUT_PROVIDER_mock         8

#endif
