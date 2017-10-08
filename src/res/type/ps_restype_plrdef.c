#include "res/ps_res_internal.h"
#include "game/ps_plrdef.h"

#define OBJ ((struct ps_plrdef*)obj)

/* Decode.
 */

static int ps_PLRDEF_decode(void *dstpp,const void *src,int srcc,int id,const char *refpath) {
  struct ps_plrdef *plrdef=ps_plrdef_new();
  if (!plrdef) return -1;
  if (ps_plrdef_decode(plrdef,src,srcc)<0) {
    ps_plrdef_del(plrdef);
    return -1;
  }
  *(void**)dstpp=plrdef;
  return 0;
}

/* Link.
 */

static int ps_PLRDEF_link(void *obj) {
  return 0;
}

/* Setup.
 */

int ps_restype_setup_PLRDEF(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_PLRDEF;
  type->name="plrdef";
  type->rid_limit=INT_MAX;

  type->del=(void*)ps_plrdef_del;
  type->decode=ps_PLRDEF_decode;
  type->link=ps_PLRDEF_link;

  return 0;
}
