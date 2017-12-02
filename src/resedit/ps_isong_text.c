#include "ps.h"
#include "ps_isong.h"
#include "util/ps_buffer.h"
#include "util/ps_text.h"

/* Encode to text, main entry point.
 */

int ps_isong_encode_text(void *dstpp,const struct ps_isong *isong) {
  if (!dstpp||!isong) return -1;
  //TODO encode isong text
  return -1;
}

/* Decode from text, main entry point.
 */

int ps_isong_decode_text(struct ps_isong *isong,const char *src,int srcc) {
  //TODO decode isong text
  return -1;
}
