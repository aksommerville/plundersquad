#include "res/ps_res_internal.h"

/* Setup.
 */

int ps_restype_setup_SONG(struct ps_restype *type) {
  if (!type) return -1;

  type->tid=PS_RESTYPE_SONG;
  type->name="song";
  type->rid_limit=INT_MAX;

  return 0;
}
