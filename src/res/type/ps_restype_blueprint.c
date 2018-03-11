#include "res/ps_res_internal.h"
#include "scenario/ps_blueprint.h"

#define OBJ ((struct ps_blueprint*)obj)

/* Decode.
 */

static int ps_BLUEPRINT_decode(void *dstpp,const void *src,int srcc,int id,const char *refpath) {
  struct ps_blueprint *blueprint=ps_blueprint_new();
  if (!blueprint) return -1;
  if (ps_blueprint_decode(blueprint,src,srcc)<0) {
    ps_blueprint_del(blueprint);
    return -1;
  }
  *(void**)dstpp=blueprint;
  return 0;
}

/* Link.
 */

static int ps_BLUEPRINT_link(void *obj) {
  return 0;
}

/* Setup.
 */

int ps_restype_setup_BLUEPRINT(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_BLUEPRINT;
  type->name="blueprint";
  type->rid_limit=INT_MAX;

  type->del=(void*)ps_blueprint_del;
  type->decode=ps_BLUEPRINT_decode;
  type->link=ps_BLUEPRINT_link;
  type->encode=(void*)ps_blueprint_encode;

  return 0;
}
