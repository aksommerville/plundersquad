/* ps_input_button.h
 * Symbols and translation for input buttons.
 */

#ifndef PS_INPUT_BUTTON_H
#define PS_INPUT_BUTTON_H

/* Player buttons. */
#define PS_PLRBTN_UP      0x0001
#define PS_PLRBTN_DOWN    0x0002
#define PS_PLRBTN_LEFT    0x0004
#define PS_PLRBTN_RIGHT   0x0008
#define PS_PLRBTN_A       0x0010
#define PS_PLRBTN_B       0x0020
#define PS_PLRBTN_PAUSE   0x0040
#define PS_PLRBTN_CD      0x8000

int ps_plrbtn_repr(char *dst,int dsta,uint16_t src);
uint16_t ps_plrbtn_eval(const char *src,int srcc);

/* Mappable actions. */
#define PS_ACTION_WARPW   0x010000
#define PS_ACTION_WARPE   0x010001
#define PS_ACTION_WARPN   0x010002
#define PS_ACTION_WARPS   0x010003

#endif
