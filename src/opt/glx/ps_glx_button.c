#include "ps_glx_internal.h"
#include <X11/keysym.h>

/* Report buttons for keyboard.
 */
 
int ps_glx_report_buttons_keyboard(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  struct ps_input_btncfg btncfg={
    .lo=0,
    .hi=2,
    .value=0,
  };
  #define _(keysym,usage) { \
    btncfg.srcbtnid=keysym; \
    btncfg.default_usage=usage; \
    if (cb(device,&btncfg,userdata)<0) return -1; \
  }
  _(XK_BackSpace,0)
  _(XK_Tab,0)
  _(XK_Linefeed,0)
  _(XK_Clear,0)
  _(XK_Return,0)
  _(XK_Pause,0)
  _(XK_Scroll_Lock,0)
  _(XK_Sys_Req,0)
  _(XK_Escape,0)
  _(XK_Delete,0)
  _(XK_Home,0)
  _(XK_Left,0)
  _(XK_Up,0)
  _(XK_Right,0)
  _(XK_Down,0)
  _(XK_Prior,0)
  _(XK_Page_Up,0)
  _(XK_Next,0)
  _(XK_Page_Down,0)
  _(XK_End,0)
  _(XK_Begin,0)
  _(XK_Select,0)
  _(XK_Print,0)
  _(XK_Execute,0)
  _(XK_Insert,0)
  _(XK_Undo,0)
  _(XK_Redo,0)
  _(XK_Menu,0)
  _(XK_Find,0)
  _(XK_Cancel,0)
  _(XK_Help,0)
  _(XK_Break,0)
  _(XK_Mode_switch,0)
  _(XK_script_switch,0)
  _(XK_Num_Lock,0)
  _(XK_KP_Space,0)
  _(XK_KP_Tab,0)
  _(XK_KP_Enter,0)
  _(XK_KP_F1,0)
  _(XK_KP_F2,0)
  _(XK_KP_F3,0)
  _(XK_KP_F4,0)
  _(XK_KP_Home,0)
  _(XK_KP_Left,0)
  _(XK_KP_Up,0)
  _(XK_KP_Right,0)
  _(XK_KP_Down,0)
  _(XK_KP_Prior,0)
  _(XK_KP_Page_Up,0)
  _(XK_KP_Next,0)
  _(XK_KP_Page_Down,0)
  _(XK_KP_End,0)
  _(XK_KP_Begin,0)
  _(XK_KP_Insert,0)
  _(XK_KP_Delete,0)
  _(XK_KP_Equal,0)
  _(XK_KP_Multiply,0)
  _(XK_KP_Add,0)
  _(XK_KP_Separator,0)
  _(XK_KP_Subtract,0)
  _(XK_KP_Decimal,0)
  _(XK_KP_Divide,0)
  _(XK_KP_0,0)
  _(XK_KP_1,0)
  _(XK_KP_2,0)
  _(XK_KP_3,0)
  _(XK_KP_4,0)
  _(XK_KP_5,0)
  _(XK_KP_6,0)
  _(XK_KP_7,0)
  _(XK_KP_8,0)
  _(XK_KP_9,0)
  _(XK_F1,0)
  _(XK_F2,0)
  _(XK_F3,0)
  _(XK_F4,0)
  _(XK_F5,0)
  _(XK_F6,0)
  _(XK_F7,0)
  _(XK_F8,0)
  _(XK_F9,0)
  _(XK_F10,0)
  _(XK_F11,0)
  _(XK_F12,0)
  _(XK_F13,0)
  _(XK_F14,0)
  _(XK_F15,0)
  _(XK_F16,0)
  _(XK_F17,0)
  _(XK_F18,0)
  _(XK_F19,0)
  _(XK_F20,0)
  _(XK_F21,0)
  _(XK_F22,0)
  _(XK_F23,0)
  _(XK_F24,0)
  _(XK_F25,0)
  _(XK_F26,0)
  _(XK_F27,0)
  _(XK_F28,0)
  _(XK_F29,0)
  _(XK_F30,0)
  _(XK_F31,0)
  _(XK_F32,0)
  _(XK_F33,0)
  _(XK_F34,0)
  _(XK_F35,0)
  _(XK_Shift_L,0)
  _(XK_Shift_R,0)
  _(XK_Control_L,0)
  _(XK_Control_R,0)
  _(XK_Caps_Lock,0)
  _(XK_Shift_Lock,0)
  _(XK_Meta_L,0)
  _(XK_Meta_R,0)
  _(XK_Alt_L,0)
  _(XK_Alt_R,0)
  _(XK_Super_L,0)
  _(XK_Super_R,0)
  _(XK_Hyper_L,0)
  _(XK_Hyper_R,0)
  _(XK_space,0)
  _(XK_exclam,0)
  _(XK_quotedbl,0)
  _(XK_numbersign,0)
  _(XK_dollar,0)
  _(XK_percent,0)
  _(XK_ampersand,0)
  _(XK_apostrophe,0)
  _(XK_quoteright,0)
  _(XK_parenleft,0)
  _(XK_parenright,0)
  _(XK_asterisk,0)
  _(XK_plus,0)
  _(XK_comma,0)
  _(XK_minus,0)
  _(XK_period,0)
  _(XK_slash,0)
  _(XK_0,0)
  _(XK_1,0)
  _(XK_2,0)
  _(XK_3,0)
  _(XK_4,0)
  _(XK_5,0)
  _(XK_6,0)
  _(XK_7,0)
  _(XK_8,0)
  _(XK_9,0)
  _(XK_colon,0)
  _(XK_semicolon,0)
  _(XK_less,0)
  _(XK_equal,0)
  _(XK_greater,0)
  _(XK_question,0)
  _(XK_at,0)
  _(XK_A,0)
  _(XK_B,0)
  _(XK_C,0)
  _(XK_D,0)
  _(XK_E,0)
  _(XK_F,0)
  _(XK_G,0)
  _(XK_H,0)
  _(XK_I,0)
  _(XK_J,0)
  _(XK_K,0)
  _(XK_L,0)
  _(XK_M,0)
  _(XK_N,0)
  _(XK_O,0)
  _(XK_P,0)
  _(XK_Q,0)
  _(XK_R,0)
  _(XK_S,0)
  _(XK_T,0)
  _(XK_U,0)
  _(XK_V,0)
  _(XK_W,0)
  _(XK_X,0)
  _(XK_Y,0)
  _(XK_Z,0)
  _(XK_bracketleft,0)
  _(XK_backslash,0)
  _(XK_bracketright,0)
  _(XK_asciicircum,0)
  _(XK_underscore,0)
  _(XK_grave,0)
  _(XK_quoteleft,0)
  _(XK_a,0)
  _(XK_b,0)
  _(XK_c,0)
  _(XK_d,0)
  _(XK_e,0)
  _(XK_f,0)
  _(XK_g,0)
  _(XK_h,0)
  _(XK_i,0)
  _(XK_j,0)
  _(XK_k,0)
  _(XK_l,0)
  _(XK_m,0)
  _(XK_n,0)
  _(XK_o,0)
  _(XK_p,0)
  _(XK_q,0)
  _(XK_r,0)
  _(XK_s,0)
  _(XK_t,0)
  _(XK_u,0)
  _(XK_v,0)
  _(XK_w,0)
  _(XK_x,0)
  _(XK_y,0)
  _(XK_z,0)
  _(XK_braceleft,0)
  _(XK_bar,0)
  _(XK_braceright,0)
  _(XK_asciitilde,0)
  #undef _
  return 0;
}

/* Report buttons for mouse.
 */
 
int ps_glx_report_buttons_mouse(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  // Honestly, I don't think we even need this... No one is using a mouse as a joystick, right?
  // Also... we're not even receiving mouse events. Hmm.
  return 0;
}

/* Codepoint from keysym.
 */
 
int ps_glx_codepoint_from_keysym(int keysym) {
  if (keysym<1) return 0;
  
  /* keysymdef.h:538 */
  if ((keysym>=0x20)&&(keysym<=0x7e)) return keysym;
  
  /* keysymdef.h:642 */
  if ((keysym>=0xa1)&&(keysym<=0xff)) return keysym;
  
  /* If the high byte is 0x01, the other 3 are Unicode.
   * I can't seem to find that documentation, but I'm pretty sure that's true.
   */
  if ((keysym&0xff000000)==0x01000000) return keysym&0x00ffffff;
  
  /* keysymdef.h:359 XK_ISO_Left_Tab, ie Shift+Tab, ie just call it Tab please.
   * Andy: Just give me a tab.
   * X11: You want a tab, kid, you got to order something.
   */
  if (keysym==0xfe20) return 0x09;
  
  /* keysymdef.h:124 */
  if (keysym==0xff0d) return 0x0a;
  if (keysym==0xffff) return 0x7f;
  if ((keysym>=0xff08)&&(keysym<=0xff1b)) return keysym-0xff00;
  
  /* keysymdef.h:202 */
  if ((keysym>=0xffb0)&&(keysym<=0xffb9)) return '0'+keysym-0xffb0;
  if ((keysym>=0xff80)&&(keysym<=0xffaf)) switch (keysym) {
    case XK_KP_Space: return 0x20;
    case XK_KP_Tab: return 0x09;
    case XK_KP_Enter: return 0x0a;
    case XK_KP_Delete: return 0x7f;
    case XK_KP_Equal: return '=';
    case XK_KP_Multiply: return '*';
    case XK_KP_Add: return '+';
    case XK_KP_Separator: return ',';
    case XK_KP_Subtract: return '-';
    case XK_KP_Decimal: return '.';
    case XK_KP_Divide: return '/';
  }
  
  return 0;
}

/* USB usage from keysym.
 * This is not exhaustive, not even close.
 * It only needs to apply when no Unicode code point is relevant.
 * I think only modifier keys matter.
 * ...You think wrong, Andy-of-the-Past. We also need F-keys, and let's throw in arrows and such.
 * Everything that isn't Unicode.
 */
 
int ps_glx_usb_usage_from_keysym(int keysym) {
  switch (keysym) {
  
    case 0xffbe: return 0x0007003a; // F1
    case 0xffbf: return 0x0007003b; // F2
    case 0xffc0: return 0x0007003c; // F3
    case 0xffc1: return 0x0007003d; // F4
    case 0xffc2: return 0x0007003e; // F5
    case 0xffc3: return 0x0007003f; // F6
    case 0xffc4: return 0x00070040; // F7
    case 0xffc5: return 0x00070041; // F8
    case 0xffc6: return 0x00070042; // F9
    case 0xffc7: return 0x00070043; // F10
    case 0xffc8: return 0x00070044; // F11
    case 0xffc9: return 0x00070045; // F12
    case 0xffca: return 0x00070068; // F13
    case 0xffcb: return 0x00070069; // F14
    case 0xffcc: return 0x0007006a; // F15
    case 0xffcd: return 0x0007006b; // F16
    case 0xffce: return 0x0007006c; // F17
    case 0xffcf: return 0x0007006d; // F18
    case 0xffd0: return 0x0007006e; // F19
    case 0xffd1: return 0x0007006f; // F20
    case 0xffd2: return 0x00070070; // F21
    case 0xffd3: return 0x00070071; // F22
    case 0xffd4: return 0x00070072; // F23
    case 0xffd5: return 0x00070073; // F24
    
    case 0xff61: return 0x00070046; // print screen
    case 0xff14: return 0x00070047; // scroll lock
    case 0xff13: return 0x00070048; // pause
    case 0xff63: return 0x00070049; // insert
    case 0xff50: return 0x0007004a; // home
    case 0xff55: return 0x0007004b; // page up
    case 0xff57: return 0x0007004d; // end
    case 0xff56: return 0x0007004e; // page down
    case 0xff53: return 0x0007004f; // right
    case 0xff51: return 0x00070050; // left
    case 0xff54: return 0x00070051; // down
    case 0xff52: return 0x00070052; // up
    case 0xff7f: return 0x00070053; // num lock
  
    case 0xffe1: return 0x000700e1; // shift l
    case 0xffe2: return 0x000700e5; // shift r
    case 0xffe3: return 0x000700e0; // control l
    case 0xffe4: return 0x000700e4; // control r
    case 0xffe5: return 0x00070039; // caps lock
    case 0xffe7: return 0x000700e3; // l meta
    case 0xffe8: return 0x000700e7; // r meta
    case 0xffe9: return 0x000700e2; // l alt
    case 0xffea: return 0x000700e6; // r alt
    case 0xffeb: return 0x000700e3; // l super
    case 0xffec: return 0x000700e7; // r super
    
  }
  return 0;
}
