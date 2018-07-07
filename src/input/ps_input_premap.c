#include "ps.h"
#include "ps_input_device.h"
#include "ps_input_premap.h"

/* Object lifecycle.
 */
 
struct ps_input_premap *ps_input_premap_new() {
  struct ps_input_premap *premap=calloc(1,sizeof(struct ps_input_premap));
  if (!premap) return 0;

  premap->refc=1;

  return premap;
}

void ps_input_premap_del(struct ps_input_premap *premap) {
  if (!premap) return;
  if (premap->refc-->1) return;

  if (premap->btncfgv) free(premap->btncfgv);

  free(premap);
}

int ps_input_premap_ref(struct ps_input_premap *premap) {
  if (!premap) return -1;
  if (premap->refc<1) return -1;
  if (premap->refc==INT_MAX) return -1;
  premap->refc++;
  return 0;
}

/* Grow buffer.
 */

static int ps_input_premap_btncfgv_require(struct ps_input_premap *premap) {
  if (premap->btncfgc<premap->btncfga) return 0;
  int na=premap->btncfga+32;
  if (na>INT_MAX/sizeof(struct ps_input_btncfg)) return -1;
  void *nv=realloc(premap->btncfgv,sizeof(struct ps_input_btncfg)*na);
  if (!nv) return -1;
  premap->btncfgv=nv;
  premap->btncfga=na;
  return 0;
}

/* Search.
 */
 
int ps_input_premap_search(const struct ps_input_premap *premap,int srcbtnid) {
  if (!premap) return -1;
  int lo=0,hi=premap->btncfgc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<premap->btncfgv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>premap->btncfgv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct ps_input_btncfg *ps_input_premap_get(const struct ps_input_premap *premap,int srcbtnid) {
  int p=ps_input_premap_search(premap,srcbtnid);
  if (p<0) return 0;
  return premap->btncfgv+p;
}

/* Insert.
 */
 
struct ps_input_btncfg *ps_input_premap_insert(struct ps_input_premap *premap,int p,int srcbtnid) {
  if (!premap) return 0;
  if ((p<0)||(p>premap->btncfgc)) {
    p=ps_input_premap_search(premap,srcbtnid);
    if (p>=0) return 0;
    p=-p-1;
  }
  if ((p>0)&&(srcbtnid<=premap->btncfgv[p-1].srcbtnid)) return 0;
  if ((p<premap->btncfgc)&&(srcbtnid>=premap->btncfgv[p].srcbtnid)) return 0;

  if (ps_input_premap_btncfgv_require(premap)<0) return 0;
  struct ps_input_btncfg *btncfg=premap->btncfgv+p;
  memmove(btncfg+1,btncfg,sizeof(struct ps_input_btncfg)*(premap->btncfgc-p));
  premap->btncfgc++;
  
  memset(btncfg,0,sizeof(struct ps_input_btncfg));
  btncfg->srcbtnid=srcbtnid;
  return btncfg;
}

/* Describe a single button.
 */
 
int ps_input_btncfg_describe(const struct ps_input_btncfg *btncfg) {
  if (!btncfg) return PS_BTNCFG_GARBAGE;
  if (btncfg->hi<=btncfg->lo) return PS_BTNCFG_GARBAGE;
  if ((btncfg->lo==0)&&(btncfg->hi<=2)) return PS_BTNCFG_TWOSTATE;
  if ((btncfg->lo==0)&&(btncfg->hi<15)&&(btncfg->default_usage==0x00010039)) return PS_BTNCFG_GARBAGE;
  if ((btncfg->lo==0)&&(btncfg->value==0)) return PS_BTNCFG_ONEWAY;
  if ((btncfg->lo<btncfg->value)&&(btncfg->hi>btncfg->value)) return PS_BTNCFG_TWOWAY;
  return PS_BTNCFG_GARBAGE;
}

/* Rebuild from device.
 */

static int ps_input_premap_cb_btncfg(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata) {
  struct ps_input_premap *premap=userdata;
  //ps_log(INPUT,DEBUG,"  %d (%d..%d) =%d [%d]",btncfg->srcbtnid,btncfg->lo,btncfg->hi,btncfg->value,btncfg->default_usage);

  int p=ps_input_premap_search(premap,btncfg->srcbtnid);
  if (p>=0) {
    ps_log(INPUT,WARN,"Ignoring duplicate button #%d on device '%.*s'.",btncfg->srcbtnid,device->namec,device->name);
    return 0;
  }
  p=-p-1;

  struct ps_input_btncfg *dst=ps_input_premap_insert(premap,p,btncfg->srcbtnid);
  if (!dst) return -1;
  memcpy(dst,btncfg,sizeof(struct ps_input_btncfg));

  switch (ps_input_btncfg_describe(btncfg)) {
    case PS_BTNCFG_GARBAGE: premap->garbagec++; break;
    case PS_BTNCFG_TWOSTATE: premap->btnc++; break;
    case PS_BTNCFG_ONEWAY: premap->axis1c++; break;
    case PS_BTNCFG_TWOWAY: premap->axis2c++; break;
  }
  
  return 0;
}

int ps_input_premap_rebuild(struct ps_input_premap *premap,struct ps_input_device *device) {
  if (!premap) return -1;
  
  premap->btncfgc=0;
  premap->btnc=0;
  premap->axis1c=0;
  premap->axis2c=0;
  premap->garbagec=0;

  if (device&&device->report_buttons) {
    //ps_log(INPUT,DEBUG,"Rebuild premap for device '%.*s'...",device->namec,device->name);
    if (device->report_buttons(device,premap,ps_input_premap_cb_btncfg)<0) return -1;
  }
  
  return 0;
}

/* Assess usability.
 */
 
int ps_input_premap_usable(const struct ps_input_premap *premap) {
  if (!premap) return 0;
  
  /* We need two axes and three buttons.
   * There's a few ways that could be formulated.
   */
  if ((premap->axis2c>=2)&&(premap->btnc>=3)) return 1; // The simplest case.
  if ((premap->axis2c>=2)&&(premap->axis1c+premap->btnc>=3)) return 1; // One-way axis can probably be used as a button.
  if (premap->btnc>=7) return 1; // Can substitute two buttons for one axis.
  if (premap->axis1c+premap->btnc>=7) return 1; // ...and I guess one-way axes are also kosher there.
  
  return 0;
}

/* Convenience ctor.
 */

int ps_input_premap_build_for_device(struct ps_input_device *device) {
  if (!device) return -1;
  if (device->premap) return 0;
  struct ps_input_premap *premap=ps_input_premap_new();
  if (!premap) return -1;
  if (ps_input_premap_rebuild(premap,device)<0) {
    ps_input_premap_del(premap);
    return -1;
  }
  device->premap=premap; // Handoff
  return 0;
}
