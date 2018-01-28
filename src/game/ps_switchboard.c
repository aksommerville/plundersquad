#include "ps.h"
#include "ps_switchboard.h"

/* Object definition.
 */

struct ps_switch {
  int switchid;
  int value;
};

struct ps_switchboard {
  int refc;
  struct ps_switch *switchv;
  int switchc,switcha;
  int (*cb)(int switchid,int value,void *userdata);
  void *userdata;
};

/* Object lifecycle.
 */
 
struct ps_switchboard *ps_switchboard_new() {
  struct ps_switchboard *switchboard=calloc(1,sizeof(struct ps_switchboard));
  if (!switchboard) return 0;

  switchboard->refc=1;

  return switchboard;
}

void ps_switchboard_del(struct ps_switchboard *switchboard) {
  if (!switchboard) return;
  if (switchboard->refc-->1) return;

  if (switchboard->switchv) free(switchboard->switchv);

  free(switchboard);
}

int ps_switchboard_ref(struct ps_switchboard *switchboard) {
  if (!switchboard) return -1;
  if (switchboard->refc<1) return -1;
  if (switchboard->refc==INT_MAX) return -1;
  switchboard->refc++;
  return 0;
}

/* Global properties.
 */

int ps_switchboard_set_callback(
  struct ps_switchboard *switchboard,
  int (*cb)(int switchid,int value,void *userdata),
  void *userdata
) {
  if (!switchboard) return -1;
  switchboard->cb=cb;
  switchboard->userdata=userdata;
  return 0;
}

/* List primitives.
 */

static int ps_switchboard_search(const struct ps_switchboard *switchboard,int switchid) {
  int lo=0,hi=switchboard->switchc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (switchid<switchboard->switchv[ck].switchid) hi=ck;
    else if (switchid>switchboard->switchv[ck].switchid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_switchboard_insert(struct ps_switchboard *switchboard,int p,int switchid) {
  if (switchboard->switchc>=switchboard->switcha) {
    int na=switchboard->switcha+8;
    if (na>INT_MAX/sizeof(struct ps_switch)) return -1;
    void *nv=realloc(switchboard->switchv,sizeof(struct ps_switch)*na);
    if (!nv) return -1;
    switchboard->switchv=nv;
    switchboard->switcha=na;
  }
  struct ps_switch *sw=switchboard->switchv+p;
  memmove(sw+1,sw,sizeof(struct ps_switch)*(switchboard->switchc-p));
  sw->switchid=switchid;
  sw->value=0;
  switchboard->switchc++;
  return 0;
}

/* Clear.
 */

int ps_switchboard_clear(struct ps_switchboard *switchboard,int fire_callbacks) {
  if (!switchboard) return -1;

  /* Trigger callback for every nonzero switch. */
  if (fire_callbacks&&switchboard->cb) {
    int i=switchboard->switchc;
    const struct ps_switch *sw=switchboard->switchv;
    for (;i-->0;sw++) {
      if (sw->value) {
        if (switchboard->cb(sw->switchid,0,switchboard->userdata)<0) return -1;
      }
    }
  }

  switchboard->switchc=0;

  return 0;
}

/* Set switch.
 */

int ps_switchboard_set_switch(struct ps_switchboard *switchboard,int switchid,int value) {
  if (!switchboard) return -1;
  if (switchid<1) return 0;
  int p=ps_switchboard_search(switchboard,switchid);
  if (p<0) {
    p=-p-1;
    if (ps_switchboard_insert(switchboard,p,switchid)<0) return -1;
  }
  value=value?1:0;
  if (value!=switchboard->switchv[p].value) {
    switchboard->switchv[p].value=value;
    if (switchboard->cb) {
      if (switchboard->cb(switchid,value,switchboard->userdata)<0) return -1;
    }
  }
  return 0;
}

/* Get switch.
 */
 
int ps_switchboard_get_switch(const struct ps_switchboard *switchboard,int switchid) {
  if (!switchboard) return 0;
  if (switchid<1) return 0;
  int p=ps_switchboard_search(switchboard,switchid);
  if (p<0) return 0;
  return switchboard->switchv[p].value;
}

/* Sequential access.
 */
 
int ps_switchboard_count_switches(const struct ps_switchboard *switchboard) {
  if (!switchboard) return 0;
  return switchboard->switchc;
}

int ps_switchboard_get_switch_by_index(int *switchid,const struct ps_switchboard *switchboard,int index) {
  if (!switchboard) return -1;
  if ((index<0)||(index>=switchboard->switchc)) return -1;
  if (switchid) *switchid=switchboard->switchv[index].switchid;
  return switchboard->switchv[index].value;
}
