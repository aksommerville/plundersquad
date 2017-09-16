#ifndef PS_INPUT_INTERNAL_H
#define PS_INPUT_INTERNAL_H

#include "ps.h"
#include "ps_input.h"
#include "ps_input_button.h"
#include "ps_input_config.h"

/* Input, global state.
 *****************************************************************************/

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

/* Private config API.
 *****************************************************************************/

struct ps_maptm_fld {
  int srcbtnid;
  int srclo,srchi;
  int dstbtnid;
};

struct ps_maptm {
  char *namepattern;
  int namepatternc;
  uint16_t vendorid,deviceid; // Zero to match any.
  int provider; // Zero to match any.
  struct ps_maptm_fld *fldv; // Sorted by (srcbtnid), duplicates permitted.
  int fldc,flda;
};

struct ps_input_config {
  char *path;
  struct ps_maptm *maptmv;
  int maptmc,maptma;
};

void ps_maptm_cleanup(struct ps_maptm *maptm);
int ps_maptm_set_namepattern(struct ps_maptm *maptm,const char *src,int srcc);
int ps_maptm_search_srcbtnid(const struct ps_maptm *maptm,int srcbtnid); // Always the first index if multiple.
int ps_maptm_insert_fld(struct ps_maptm *maptm,int p,int srcbtnid,int srclo,int srchi,int dstbtnid); // p OOB to detect

/* Consider these match criteria.
 * Returns >0 for a match, 0 if no match, never <0.
 * Higher results are more precise matches.
 */
int ps_maptm_match_device(const struct ps_maptm *maptm,const char *name,int namec,uint16_t vendorid,uint16_t deviceid,int provider);

/* Private general input API.
 *****************************************************************************/

int ps_input_keymap_search(int keycode);
int ps_input_joymap_search(int provider,int devid,int btnid);
struct ps_keymap *ps_input_keymap_insert(int p,int keycode);
struct ps_joymap *ps_input_joymap_insert(int p,int provider,int devid,int btnid);
int ps_input_joymap_remove_device(int provider,int devid);
int ps_joymap_apply(struct ps_joymap *joymap,int value); // Nonzero if (joymap->dstpv) changed.

#endif
