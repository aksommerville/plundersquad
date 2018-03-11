#include "res/ps_res_internal.h"
#include "game/ps_sprite.h"

/* Decode.
 */

static int _ps_restype_SPRDEF_decode(void *objpp,const void *src,int srcc,int id,const char *refpath) {
  struct ps_sprdef *sprdef=ps_sprdef_decode(src,srcc);
  if (!sprdef) return -1;
  *(void**)objpp=sprdef;
  return 0;
}

/* Encode.
 */
 
static int _ps_restype_SPRDEF_encode(void *dst,int dsta,const void *obj) {
  //TODO add a binary format for sprdef and replace this:
  void *tmp=0;
  int dstc=ps_sprdef_encode(&tmp,obj);
  if (dstc<0) return 0;
  if (dstc<=dsta) memcpy(dst,tmp,dstc);
  if (tmp) free(tmp);
  return dstc;
}

/* Setup.
 */

int ps_restype_setup_SPRDEF(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_SPRDEF;
  type->name="sprdef";
  type->rid_limit=INT_MAX;

  type->del=(void*)ps_sprdef_del;
  type->decode=_ps_restype_SPRDEF_decode;
  type->encode=_ps_restype_SPRDEF_encode;

  return 0;
}
