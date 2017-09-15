#ifndef PS_INPUT_INTERNAL_H
#define PS_INPUT_INTERNAL_H

#include "ps.h"
#include "ps_input.h"
#include "ps_input_button.h"

struct ps_keymap {
  int keycode;
  int plrid;
  int btnid;
};

struct ps_joymap {
  int provider;
  int srcdevid;
  int srcbtnid;
  int dstplrid;
  int dstbtnid;
  int srcpv,dstpv;
  int srclo,srchi;
};

extern struct ps_input {
  int init;
  uint16_t plrbtnv[1+PS_PLAYER_LIMIT];
  
  struct ps_keymap *keymapv;
  int keymapc,keymapa;

  struct ps_joymap *joymapv;
  int joymapc,joymapa;
  
} ps_input;

int ps_input_keymap_search(int keycode);
int ps_input_joymap_search(int provider,int devid,int btnid);
struct ps_keymap *ps_input_keymap_insert(int p,int keycode);
struct ps_joymap *ps_input_joymap_insert(int p,int provider,int devid,int btnid);
int ps_input_joymap_remove_device(int provider,int devid);
int ps_joymap_apply(struct ps_joymap *joymap,int value); // Nonzero if (joymap->dstpv) changed.

#endif
