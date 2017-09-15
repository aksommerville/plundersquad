#include "ps.h"
#include "ps_input_button.h"
#include "util/ps_text.h"

/* Player button names.
 */
 
int ps_plrbtn_repr(char *dst,int dsta,uint16_t src) {
  if (!dst||(dsta<0)) dsta=0;
  switch (src) {
    #define _(tag) case PS_PLRBTN_##tag: { \
        if (dsta>=sizeof(#tag)-1) { \
          memcpy(dst,#tag,sizeof(#tag)-1); \
          if (dsta>sizeof(#tag)-1) dst[sizeof(#tag)-1]=0; \
        } \
        return sizeof(#tag)-1; \
      }
    _(UP)
    _(DOWN)
    _(LEFT)
    _(RIGHT)
    _(A)
    _(B)
    _(PAUSE)
    _(CD)
    #undef _
  }
  return -1;
}

uint16_t ps_plrbtn_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) \
    if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_PLRBTN_##tag;
  _(UP)
  _(DOWN)
  _(LEFT)
  _(RIGHT)
  _(A)
  _(B)
  _(PAUSE)
  _(CD)
  #undef _
  return 0;
}
