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

/* Rebuild from device.
 */

static int ps_input_premap_cb_btncfg(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata) {
  struct ps_input_premap *premap=userdata;

  int p=ps_input_premap_search(premap,btncfg->srcbtnid);
  if (p>=0) {
    ps_log(INPUT,WARN,"Ignoring duplicate button #%d on device '%.*s'.",btncfg->srcbtnid,device->namec,device->name);
    return 0;
  }
  p=-p-1;

  struct ps_input_btncfg *dst=ps_input_premap_insert(premap,p,btncfg->srcbtnid);
  if (!dst) return -1;
  memcpy(dst,btncfg,sizeof(struct ps_input_btncfg));
  
  if (btncfg->hi<=btncfg->lo) {
    premap->garbagec++;
  } else if ((btncfg->lo==0)&&(btncfg->hi<=2)) { // buttons can have a hi of 1 or 2 (keyboards may use 2, for autorepeat)
    premap->btnc++;
  } else if ((btncfg->lo==0)&&(btncfg->value==0)) {
    premap->axis1c++;
  } else {
    premap->axis2c++;
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
    if (device->report_buttons(device,premap,ps_input_premap_cb_btncfg)<0) return -1;
  }
  
  return 0;
}
