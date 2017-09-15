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
 * TODO: Assert existence of tilesheet and song.
 */

static int ps_REGION_link(void *obj) {
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
