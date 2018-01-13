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

// These aggregate markers are only for ps_input_btncfg.default_usage
#define PS_PLRBTN_HORZ   (PS_PLRBTN_LEFT|PS_PLRBTN_RIGHT)
#define PS_PLRBTN_VERT   (PS_PLRBTN_UP|PS_PLRBTN_DOWN)
#define PS_PLRBTN_THUMB  (PS_PLRBTN_A|PS_PLRBTN_B)
#define PS_PLRBTN_AUX    (PS_PLRBTN_START)

#define PS_ACTION_WARPN         0x00010001
#define PS_ACTION_WARPS         0x00010002
#define PS_ACTION_WARPW         0x00010003
#define PS_ACTION_WARPE         0x00010004
#define PS_ACTION_PAUSE         0x00010005 /* toggle */
#define PS_ACTION_PAUSEON       0x00010006
#define PS_ACTION_PAUSEOFF      0x00010007
#define PS_ACTION_FULLSCREEN    0x00010008 /* toggle */
#define PS_ACTION_FULLSCREENON  0x00010009
#define PS_ACTION_FULLSCREENOFF 0x0001000a
#define PS_ACTION_QUIT          0x0001000b
#define PS_ACTION_SCREENSHOT    0x0001000c
#define PS_ACTION_DEBUG         0x0001000d

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

const char *ps_plrbtn_repr(int btnid);
int ps_plrbtn_eval(const char *src,int srcc);
const char *ps_action_repr(int action);
int ps_action_eval(const char *src,int srcc);

int ps_btnid_repr(char *dst,int dsta,int btnid);
int ps_btnid_eval(const char *src,int srcc);

int ps_input_provider_repr(char *dst,int dsta,int providerid);
int ps_input_provider_eval(const char *src,int srcc);

int ps_input_hid_usage_repr(char *dst,int dsta,int usage);
int ps_input_hid_usage_eval(int *usage,const char *src,int srcc);

#endif
