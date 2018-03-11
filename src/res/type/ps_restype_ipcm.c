#include "ps.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "akau/akau.h"
#include "akau/akau_store.h"
#include "akau/akau_pcm.h"

/* Fallback mode: keep all serialized data.
 */
 
static int ps_ipcm_fallback=0;
static int ps_ipcm_activity=0;

int ps_res_ipcm_use_fallback() {
  if (ps_ipcm_activity) return -1;
  ps_ipcm_fallback=1;
  return 0;
}

/* Decode.
 */
 
static int ps_IPCM_decode(void *objpp,const void *src,int srcc,int id,const char *path) {

  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  
  struct akau_ipcm *ipcm=akau_ipcm_decode(src,srcc);
  if (!ipcm) return -1;
    
  if (akau_store_add_ipcm(store,ipcm,id)<0) {
    akau_ipcm_del(ipcm);
    return -1;
  }
  
  *(void**)objpp=ipcm;
  return 0;
}

static int ps_IPCM_decode_fallback(void *objpp,const void *src,int srcc,int id,const char *path) {
  void *serial=malloc(4+srcc);
  if (!serial) return -1;
  *(uint32_t*)serial=srcc;
  memcpy((char*)serial+4,src,srcc);
  *(void**)objpp=serial;
  return 0;
}

/* Link.
 */
 
static int ps_IPCM_link(void *obj) {
  return 0;
}

/* Encode.
 */
 
static int ps_IPCM_encode(void *dst,int dsta,const void *obj) {
  if (!ps_ipcm_fallback) return -1;
  if (!obj) return -1;
  uint32_t srcc=*(uint32_t*)obj;
  if (srcc<=dsta) {
    memcpy(dst,(char*)obj+4,srcc);
  }
  return srcc;
}

/* Setup new type.
 */
 
int ps_restype_setup_IPCM(struct ps_restype *type) {
  if (!type) return -1;
  ps_ipcm_activity=1;
  
  type->tid=PS_RESTYPE_IPCM;
  type->name="ipcm";
  type->rid_limit=INT_MAX;
  
  if (ps_ipcm_fallback) {
    type->del=free;
    type->decode=ps_IPCM_decode_fallback;
    type->encode=ps_IPCM_encode;
  } else {
    type->del=(void*)akau_ipcm_del;
    type->decode=ps_IPCM_decode;
    type->link=ps_IPCM_link;
  }
  
  return 0;
}
