#include "ps_input_internal.h"

struct ps_input ps_input={0};

/* Init.
 */

int ps_input_init() {

  if (ps_input.init) return -1;
  memset(&ps_input,0,sizeof(struct ps_input));
  ps_input.init=1;

  return 0;
}

/* Quit.
 */
 
void ps_input_quit() {
  if (ps_input.keymapv) free(ps_input.keymapv);
  if (ps_input.joymapv) free(ps_input.joymapv);
  memset(&ps_input,0,sizeof(struct ps_input));
}

/* Default key map.
 */
 
int ps_input_set_default_keyboard_mapping() {

  /* Player 1: arrows, z, x, enter */
  if (ps_input_map_key(0x0007004f,1,PS_PLRBTN_RIGHT)<0) return -1; // arrow
  if (ps_input_map_key(0x00070050,1,PS_PLRBTN_LEFT)<0) return -1; // arrow
  if (ps_input_map_key(0x00070051,1,PS_PLRBTN_DOWN)<0) return -1; // arrow
  if (ps_input_map_key(0x00070052,1,PS_PLRBTN_UP)<0) return -1; // arrow
  if (ps_input_map_key(0x00070028,1,PS_PLRBTN_PAUSE)<0) return -1; // ENTER
  if (ps_input_map_key(0x0007001d,1,PS_PLRBTN_A)<0) return -1; // Z
  if (ps_input_map_key(0x0007001b,1,PS_PLRBTN_B)<0) return -1; // X

  /* Player 2: e,s,d,f,w,r */
  if (ps_input_map_key(0x00070009,2,PS_PLRBTN_RIGHT)<0) return -1;
  if (ps_input_map_key(0x00070016,2,PS_PLRBTN_LEFT)<0) return -1;
  if (ps_input_map_key(0x00070007,2,PS_PLRBTN_DOWN)<0) return -1;
  if (ps_input_map_key(0x00070008,2,PS_PLRBTN_UP)<0) return -1;
  if (ps_input_map_key(0x0007001a,2,PS_PLRBTN_A)<0) return -1;
  if (ps_input_map_key(0x00070015,2,PS_PLRBTN_B)<0) return -1;

  /* Player 3: i,j,k,l,u,o */
  if (0) {
    if (ps_input_map_key(0x0007000f,3,PS_PLRBTN_RIGHT)<0) return -1;
    if (ps_input_map_key(0x0007000d,3,PS_PLRBTN_LEFT)<0) return -1;
    if (ps_input_map_key(0x0007000e,3,PS_PLRBTN_DOWN)<0) return -1;
    if (ps_input_map_key(0x0007000c,3,PS_PLRBTN_UP)<0) return -1;
    if (ps_input_map_key(0x00070018,3,PS_PLRBTN_A)<0) return -1;
    if (ps_input_map_key(0x00070012,3,PS_PLRBTN_B)<0) return -1;
  } else { // Or use ijkl for screen warp
    if (ps_input_map_key(0x0007000f,3,PS_ACTION_WARPE)<0) return -1;
    if (ps_input_map_key(0x0007000d,3,PS_ACTION_WARPW)<0) return -1;
    if (ps_input_map_key(0x0007000e,3,PS_ACTION_WARPS)<0) return -1;
    if (ps_input_map_key(0x0007000c,3,PS_ACTION_WARPN)<0) return -1;
    if (ps_input_map_key(0x00070018,3,PS_PLRBTN_A)<0) return -1;
    if (ps_input_map_key(0x00070012,3,PS_PLRBTN_B)<0) return -1;
  }
  
  return 0;
}

/* Set mappings, high level.
 */
 
int ps_input_map_key(int keycode,int plrid,int dstbtnid) {
  int p=ps_input_keymap_search(keycode);
  if (p>=0) return -1;
  p=-p-1;
  struct ps_keymap *keymap=ps_input_keymap_insert(p,keycode);
  if (!keymap) return -1;
  keymap->plrid=plrid;
  keymap->btnid=dstbtnid;
  return 0;
}

int ps_input_map_jbtn(int provider,int devid,int srcbtnid,int lo,int hi,int plrid,int dstbtnid) {
  int p=ps_input_joymap_search(provider,devid,srcbtnid);
  if (p>=0) return -1;
  p=-p-1;
  struct ps_joymap *joymap=ps_input_joymap_insert(p,provider,devid,srcbtnid);
  if (!joymap) return -1;
  joymap->srclo=lo;
  joymap->srchi=hi;
  joymap->dstplrid=plrid;
  joymap->dstbtnid=dstbtnid;
  return 0;
}

/* Update.
 */

int ps_input_update() {
  if (!ps_input.init) return -1;
  return 0;
}

/* Player buttons.
 */
 
uint16_t ps_get_player_buttons(int plrid) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return 0;
  return ps_input.plrbtnv[plrid];
}

int ps_get_player_button(int plrid,uint16_t btnid) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return 0;
  return (ps_input.plrbtnv[plrid]&btnid)?1:0;
}

int ps_set_player_button(int plrid,uint16_t btnid,int value) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return -1;
  
  if (value) {
    if (ps_input.plrbtnv[plrid]&btnid) return 0;
    ps_input.plrbtnv[plrid]|=btnid;
  } else {
    if (!(ps_input.plrbtnv[plrid]&btnid)) return 0;
    ps_input.plrbtnv[plrid]&=~btnid;
  }

  /* Update player zero. */
  if (plrid) {
    if (value) ps_input.plrbtnv[0]|=btnid;
    else ps_input.plrbtnv[0]&=~btnid;
  }

  return 1;
}

int ps_clear_player_buttons(int plrid) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return -1;
  if (plrid) {
    ps_input.plrbtnv[plrid]=0;
  } else {
    memset(ps_input.plrbtnv,0,sizeof(ps_input.plrbtnv));
  }
  return 0;
}

/* General one-shot actions.
 */

int ps_input_fire_action(int actionid) {
  ps_log(INPUT,INFO,"Fire action 0x%x.",actionid);
  return ps_main_input_action_callback(actionid);
}

/* Search maps.
 */
 
int ps_input_keymap_search(int keycode) {
  int lo=0,hi=ps_input.keymapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (keycode<ps_input.keymapv[ck].keycode) hi=ck;
    else if (keycode>ps_input.keymapv[ck].keycode) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ps_input_joymap_search(int provider,int devid,int btnid) {
  int lo=0,hi=ps_input.joymapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (provider<ps_input.joymapv[ck].provider) hi=ck;
    else if (provider>ps_input.joymapv[ck].provider) lo=ck+1;
    else if (devid<ps_input.joymapv[ck].srcdevid) hi=ck;
    else if (devid>ps_input.joymapv[ck].srcdevid) lo=ck+1;
    else if (btnid<ps_input.joymapv[ck].srcbtnid) hi=ck;
    else if (btnid>ps_input.joymapv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Reallocate maps.
 */

static int ps_input_keymap_require() {
  if (!ps_input.init) return -1;
  if (ps_input.keymapc<ps_input.keymapa) return 0;
  int na=ps_input.keymapa+32;
  if (na>INT_MAX/sizeof(struct ps_keymap)) return -1;
  void *nv=realloc(ps_input.keymapv,sizeof(struct ps_keymap)*na);
  if (!nv) return -1;
  ps_input.keymapv=nv;
  ps_input.keymapa=na;
  return 0;
}

static int ps_input_joymap_require() {
  if (!ps_input.init) return -1;
  if (ps_input.joymapc<ps_input.joymapa) return 0;
  int na=ps_input.joymapa+32;
  if (na>INT_MAX/sizeof(struct ps_joymap)) return -1;
  void *nv=realloc(ps_input.joymapv,sizeof(struct ps_joymap)*na);
  if (!nv) return -1;
  ps_input.joymapv=nv;
  ps_input.joymapa=na;
  return 0;
}

/* Insert maps.
 */
 
struct ps_keymap *ps_input_keymap_insert(int p,int keycode) {
  if (ps_input_keymap_require()<0) return 0;
  if ((p<0)||(p>ps_input.keymapc)) return 0;
  
  if ((p>0)&&(keycode<=ps_input.keymapv[p-1].keycode)) return 0;
  if ((p<ps_input.keymapc)&&(keycode>=ps_input.keymapv[p].keycode)) return 0;

  struct ps_keymap *keymap=ps_input.keymapv+p;
  memmove(keymap+1,keymap,sizeof(struct ps_keymap)*(ps_input.keymapc-p));
  ps_input.keymapc++;
  memset(keymap,0,sizeof(struct ps_keymap));
  keymap->keycode=keycode;
  return keymap;
}

struct ps_joymap *ps_input_joymap_insert(int p,int provider,int devid,int btnid) {
  if (ps_input_joymap_require()<0) return 0;
  if ((p<0)||(p>ps_input.joymapc)) return 0;

  if (p>0) {
    if (provider<ps_input.joymapv[p-1].provider) return 0;
    else if (provider==ps_input.joymapv[p-1].provider) {
      if (devid<ps_input.joymapv[p-1].srcdevid) return 0;
      else if (devid==ps_input.joymapv[p-1].srcdevid) {
        if (btnid<=ps_input.joymapv[p-1].srcbtnid) return 0;
      }
    }
  }
  if (p<ps_input.joymapc) {
    if (provider>ps_input.joymapv[p].provider) return 0;
    else if (provider==ps_input.joymapv[p].provider) {
      if (devid>ps_input.joymapv[p].srcdevid) return 0;
      else if (devid==ps_input.joymapv[p].srcdevid) {
        if (btnid>=ps_input.joymapv[p].srcbtnid) return 0;
      }
    }
  }

  struct ps_joymap *joymap=ps_input.joymapv+p;
  memmove(joymap+1,joymap,sizeof(struct ps_joymap)*(ps_input.joymapc-p));
  ps_input.joymapc++;
  memset(joymap,0,sizeof(struct ps_joymap));
  joymap->provider=provider;
  joymap->srcdevid=devid;
  joymap->srcbtnid=btnid;
  return joymap;
}

/* Remove joymaps for a given device.
 */
 
int ps_input_joymap_remove_device(int provider,int devid) {
  int pa=ps_input_joymap_search(provider,devid,INT_MIN);
  if (pa<0) pa=-pa-1;
  int pz=ps_input_joymap_search(provider,devid+1,INT_MIN);
  if (pz<0) pz=-pz-1;
  int c=pz-pa;
  if (c<1) return 0;
  ps_input.joymapc-=c;
  memmove(ps_input.joymapv+pa,ps_input.joymapv+pa+c,sizeof(struct ps_joymap)*(ps_input.joymapc-pa));
  return 0;
}

/* Apply value to joymap.
 */
 
int ps_joymap_apply(struct ps_joymap *joymap,int value) {
  if (!joymap) return 0;
  if (joymap->srcpv==value) return 0;
  joymap->srcpv=value;
  int dstvalue;
  if (value<joymap->srclo) dstvalue=0;
  else if (value>joymap->srchi) dstvalue=0;
  else dstvalue=1;
  if (joymap->dstpv==dstvalue) return 0;
  joymap->dstpv=dstvalue;
  return 1;
}

/* Provider hooks.
 */
 
int ps_input_rcvevt_focus(int focus) {
  ps_log(INPUT,TRACE,"focus %d",focus);
  return 0;
}

int ps_input_rcvevt_key(int keycode,int codepoint,int value) {
  ps_log(INPUT,TRACE,"key %d[U+%x]=%d",keycode,codepoint,value);

  int mapp=ps_input_keymap_search(keycode);
  if (mapp>=0) {
    struct ps_keymap *keymap=ps_input.keymapv+mapp;
    if (keymap->btnid&0xffff0000) {
      if (value) {
        if (ps_input_fire_action(keymap->btnid)<0) return -1;
      }
    } else {
      if (ps_set_player_button(keymap->plrid,keymap->btnid,value)<0) return -1;
    }
  }
  
  return 0;
}

int ps_input_rcvevt_mmotion(int x,int y) {
  //ps_log(INPUT,TRACE,"mmotion %d,%d",x,y);
  return 0;
}

int ps_input_rcvevt_mbutton(int btnid,int value) {
  ps_log(INPUT,TRACE,"mbutton %d=%d",btnid,value);
  return 0;
}

int ps_input_rcvevt_mwheel(int dx,int dy) {
  ps_log(INPUT,TRACE,"mwheel %+d,%+d",dx,dy);
  return 0;
}

int ps_input_rcvevt_jconnect(int provider,int devid) {
  ps_log(INPUT,TRACE,"jconnect %d:%d",provider,devid);
  //TODO setup maps for joystick
  return 0;
}

int ps_input_rcvevt_jdisconnect(int provider,int devid) {
  ps_log(INPUT,TRACE,"jdisconnect %d:%d",provider,devid);
  if (ps_input_joymap_remove_device(provider,devid)<0) return -1;
  return 0;
}

int ps_input_rcvevt_jbutton(int provider,int devid,int btnid,int value) {
  ps_log(INPUT,TRACE,"jbutton %d:%d.%d=%d",provider,devid,btnid,value);

  int mapp=ps_input_joymap_search(provider,devid,btnid);
  if (mapp>=0) {
    struct ps_joymap *joymap=ps_input.joymapv+mapp;
    if (ps_joymap_apply(joymap,value)) {
      if (joymap->dstbtnid&0xffff0000) {
        if (joymap->dstpv) {
          if (ps_input_fire_action(joymap->dstbtnid)<0) return -1;
        }
      } else {
        if (ps_set_player_button(joymap->dstplrid,joymap->dstbtnid,joymap->dstpv)<0) return -1;
      }
    }
  }
  
  return 0;
}
