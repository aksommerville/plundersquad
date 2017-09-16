#include "ps.h"
#include "ps_input_button.h"
#include "util/ps_text.h"

/* Player buttons.
 */

const char *ps_plrbtn_repr(int btnid) {
  switch (btnid) {
    #define _(tag) case PS_PLRBTN_##tag: return #tag;
    _(UP)
    _(DOWN)
    _(LEFT)
    _(RIGHT)
    _(A)
    _(B)
    _(START)
    _(HORZ)
    _(VERT)
    _(THUMB)
    //_(AUX)
    #undef _
  }
  return 0;
}

int ps_plrbtn_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_PLRBTN_##tag;
  _(UP)
  _(DOWN)
  _(LEFT)
  _(RIGHT)
  _(A)
  _(B)
  _(START)
  _(HORZ)
  _(VERT)
  _(THUMB)
  //_(AUX)
  #undef _
  return -1;
}

/* Actions.
 */
 
const char *ps_action_repr(int action) {
  switch (action) {
    #define _(tag) case PS_ACTION_##tag: return #tag;
    _(WARPN)
    _(WARPS)
    _(WARPW)
    _(WARPE)
    _(PAUSE)
    _(PAUSEON)
    _(PAUSEOFF)
    _(FULLSCREEN)
    _(FULLSCREENON)
    _(FULLSCREENOFF)
    _(QUIT)
    _(SCREENSHOT)
    _(DEBUG)
    #undef _
  }
  return 0;
}

int ps_action_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    #define _(tag) if (!ps_memcasecmp(src,#tag,srcc)) return PS_ACTION_##tag;
    case 4: {
        _(QUIT)
      } break;
    case 5: {
        _(WARPN)
        _(WARPS)
        _(WARPW)
        _(WARPE)
        _(PAUSE)
        _(DEBUG)
      } break;
    case 7: {
        _(PAUSEON)
      } break;
    case 8: {
        _(PAUSEOFF)
      } break;
    case 10: {
        _(FULLSCREEN)
        _(SCREENSHOT)
      } break;
    case 12: {
        _(FULLSCREENON)
      } break;
    case 13: {
        _(FULLSCREENOFF)
      } break;
    #undef _
  }
  return -1;
}

/* Evaluate/represent btnid (player or action).
 */

int ps_btnid_repr(char *dst,int dsta,int btnid) {
  const char *src;
  if (src=ps_plrbtn_repr(btnid)) return ps_strcpy(dst,dsta,src,-1);
  if (src=ps_action_repr(btnid)) return ps_strcpy(dst,dsta,src,-1);
  return ps_decsint_repr(dst,dsta,btnid);
}

int ps_btnid_eval(const char *src,int srcc) {
  int btnid;
  if ((btnid=ps_plrbtn_eval(src,srcc))>0) return btnid;
  if ((btnid=ps_action_eval(src,srcc))>0) return btnid;
  if (ps_int_eval(&btnid,src,srcc)>=0) return btnid;
  return -1;
}

/* Provider names.
 */

int ps_input_provider_repr(char *dst,int dsta,int providerid) {
  switch (providerid) {
    #define _(tag) case PS_INPUT_PROVIDER_##tag: return ps_strcpy(dst,dsta,#tag,-1);
    _(macioc)
    _(macwm)
    _(machid)
    _(evdev)
    _(x11)
    _(mswm)
    _(mshid)
    #undef _
  }
  return ps_decsint_repr(dst,dsta,providerid);
}

int ps_input_provider_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  if ((srcc==5)&&!ps_memcasecmp(src,"macwm",5)) return PS_INPUT_PROVIDER_macwm;
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!ps_memcasecmp(src,#tag,srcc)) return PS_INPUT_PROVIDER_##tag;
  _(macioc)
  _(macwm)
  _(machid)
  _(evdev)
  _(x11)
  _(mswm)
  _(mshid)
  #undef _

  int providerid;
  if (ps_int_eval(&providerid,src,srcc)>=0) return providerid;
  return -1;
}

/* HID usage codes.
 */
 
int ps_input_hid_usage_repr(char *dst,int dsta,int usage) {
  //TODO HID usage text
  return -1;
}

int ps_input_hid_usage_eval(int *usage,const char *src,int srcc) {
  //TODO HID usage text
  return -1;
}
