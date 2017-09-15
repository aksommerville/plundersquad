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

/* Setup.
 */

int ps_restype_setup_SPRDEF(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_SPRDEF;
  type->name="sprdef";
  type->rid_limit=INT_MAX;

  type->del=(void*)ps_sprdef_del;
  type->decode=_ps_restype_SPRDEF_decode;

  return 0;
}
