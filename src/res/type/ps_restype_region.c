#include "res/ps_res_internal.h"
#include "scenario/ps_region.h"

#define OBJ ((struct ps_region*)obj)
  

/* Decode.
 */

static int ps_REGION_decode(void *dstpp,const void *src,int srcc,int id,const char *refpath) {
  struct ps_region *region=ps_region_decode(src,srcc);
  if (!region) return -1;
  *(void**)dstpp=region;
  return 0;
}

/* Link.
 */

static int ps_REGION_link(void *obj) {
  struct ps_region *region=obj;

  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,region->tsid);
  if (!tilesheet) {
    ps_log(RES,ERROR,"tilesheet:%d not found",region->tsid);
    return -1;
  }

  if (region->songid) {
    struct ps_res_SONG *song=ps_res_get(PS_RESTYPE_SONG,region->songid);
    if (!song) {
      ps_log(RES,ERROR,"song:%d not found",region->songid);
      return -1;
    }
  }

  int i=PS_REGION_MONSTER_LIMIT;
  while (i-->0) {
    int sprdefid=region->monster_sprdefidv[i];
    if (!sprdefid) continue;
    struct ps_sprdef *sprdef=ps_res_get(PS_RESTYPE_SPRDEF,sprdefid);
    if (!sprdef) {
      ps_log(RES,ERROR,"sprdef:%d not found",sprdefid);
      return -1;
    }
  }
  
  return 0;
}

/* Setup.
 */

int ps_restype_setup_REGION(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_REGION;
  type->name="region";
  type->rid_limit=INT_MAX;

  type->del=(void*)ps_region_del;
  type->decode=ps_REGION_decode;
  type->link=ps_REGION_link;

  return 0;
}
