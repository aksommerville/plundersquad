#include "ps.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "akau/akau.h"
#include "akau/akau_store.h"
#include "akau/akau_song.h"

/* Fallback mode: keep all serialized data.
 */
 
static int ps_song_fallback=0;
static int ps_song_activity=0;

int ps_res_song_use_fallback() {
  if (ps_song_activity) return -1;
  ps_song_fallback=1;
  return 0;
}

/* Decode.
 */
 
static int ps_SONG_decode(void *objpp,const void *src,int srcc,int id,const char *path) {

  struct akau_store *store=akau_get_store();
  if (!store) return -1;
  
  struct akau_song *song=akau_song_new();
  if (!song) return -1;
  if (akau_song_decode(song,src,srcc)<0) {
    akau_song_del(song);
    return -1;
  }
    
  if (akau_store_add_song(store,song,id)<0) {
    akau_song_del(song);
    return -1;
  }
  
  *(void**)objpp=song;
  return 0;
}

static int ps_SONG_decode_fallback(void *objpp,const void *src,int srcc,int id,const char *path) {
  void *serial=malloc(4+srcc);
  if (!serial) return -1;
  *(uint32_t*)serial=srcc;
  memcpy((char*)serial+4,src,srcc);
  *(void**)objpp=serial;
  return 0;
}

/* Link.
 */
 
static int ps_SONG_link(void *obj) {
  if (akau_song_link(obj,akau_get_store())<0) return -1;
  return 0;
}

/* Encode.
 */
 
static int ps_SONG_encode(void *dst,int dsta,const void *obj) {
  if (!ps_song_fallback) return -1;
  if (!obj) return -1;
  uint32_t srcc=*(uint32_t*)obj;
  if (srcc<=dsta) {
    memcpy(dst,(char*)obj+4,srcc);
  }
  return srcc;
}

/* Setup new type.
 */
 
int ps_restype_setup_SONG(struct ps_restype *type) {
  if (!type) return -1;
  ps_song_activity=1;
  
  type->tid=PS_RESTYPE_SONG;
  type->name="song";
  type->rid_limit=INT_MAX;
  
  if (ps_song_fallback) {
    type->del=free;
    type->decode=ps_SONG_decode_fallback;
    type->encode=ps_SONG_encode;
  } else {
    type->del=(void*)akau_song_del;
    type->decode=ps_SONG_decode;
    type->link=ps_SONG_link;
  }
  
  return 0;
}
