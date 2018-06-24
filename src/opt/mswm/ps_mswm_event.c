#include "ps_mswm_internal.h"
#include "opt/mshid/ps_mshid.h"
#include "video/ps_video.h"

/* Window resized.
 */

static int ps_mswm_cb_resize(int hint,int w,int h) {
  ps_mswm.winw=w;
  ps_mswm.winh=h;
  if (ps_video_resized(w,h)<0) return -1;
  return 0;
}

/* Activate.
 */

static int ps_mswm_cb_activate(int state) {
  ps_log(MSWM,TRACE,"%s %d",__func__,state);
  //TODO Should we do something on window focus, like pause the game?
  return 0;
}

/* Keyboard.
 */

static int ps_mswm_evt_key(WPARAM wparam,LPARAM lparam) {
  //ps_log(MSWM,TRACE,"%s %d %d",__func__,wparam,lparam);
  int value;
  if (lparam&0x80000000) { // up
    value=0;
  } else if (lparam&0x40000000) { // repeat
    value=2;
  } else { // down
    value=1;
  }
  if (ps_input_event_button(ps_mswm.dev_keyboard,wparam,value)<0) return -1;
  if (ps_input_event_key(wparam,0,value)<0) return -1;
  return 0;
}

static int ps_mswm_evt_char(int codepoint,int lparam) {
  //ps_log(MSWM,TRACE,"%s U+%x [%08x]",__func__,codepoint,lparam);
  int value;
  if (lparam&0x80000000) return 0;
  if (lparam&0x40000000) {
    value=2;
  } else {
    value=1;
  }
  if (ps_input_event_key(0,codepoint,value)<0) return -1;
  return 0;
}

/* Mouse.
 */

static int ps_mswm_evt_mbtn(int btnid,int state) {
  //ps_log(MSWM,TRACE,"%s %d=%d",__func__,btnid,state);
  if (ps_input_event_mbutton(btnid,state)<0) return -1;
  return 0;
}

static int ps_mswm_evt_mwheel(int wparam,int lparam,int horz) {
  int16_t d=(wparam>>16)/WHEEL_DELTA;
  d=-d; // Positive is away from the user, ie up. That's backwards to my preference.
  if (wparam&MK_SHIFT) { // Use SHIFT key to toggle axis.
    horz=!horz;
  }
  if (horz) {
    if (ps_input_event_mwheel(d,0)<0) return -1;
  } else {
    if (ps_input_event_mwheel(0,d)<0) return -1;
  }
  return 0;
}

static int ps_mswm_evt_mmove(int x,int y) {
  //ps_log(MSWM,TRACE,"%s %d,%d",__func__,x,y);
  if (ps_input_event_mmotion(x,y)<0) return -1;
  return 0;
}

/* Receive event.
 */

LRESULT ps_mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) {
  //ps_log(MSWM,TRACE,"%s %d %d %d",__func__,msg,wparam,lparam);
  switch (msg) {
  
    case WM_CREATE: {
        if (!hwnd) return -1;
        ps_mswm.hwnd=hwnd;
        if (ps_mswm_setup_window()<0) return -1;
      } return 0;
      
    case WM_DESTROY: {
        if (ps_input_request_termination()<0) return -1;
      } return 0;

    /* Hook into these for "are you sure?" dialogue. */
    case WM_QUIT:
    case WM_CLOSE: break;

    //TODO We receive WM_SIZE when the resize is complete. Can we get it on the fly too?
    case WM_SIZE: {
        if (hwnd!=ps_mswm.hwnd) return 0;
        if (ps_mswm_cb_resize(wparam,LOWORD(lparam),HIWORD(lparam))<0) return -1;
      } return 0;

    // Comments on MSDN suggest we should check both WM_ACTIVATEAPP and WM_NCACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE: {
        if (hwnd!=ps_mswm.hwnd) return 0;
        if (ps_mswm_cb_activate(wparam)<0) return -1;
      } return 0;

    case WM_CHAR: {
        if (ps_mswm_evt_char(wparam,lparam)<0) return -1;
      } return 0;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: break;
    case WM_KEYDOWN:
    case WM_KEYUP: {
        return ps_mswm_evt_key(wparam,lparam);
      }

    case WM_LBUTTONDOWN: return ps_mswm_evt_mbtn(1,1);
    case WM_LBUTTONUP: return ps_mswm_evt_mbtn(1,0);
    case WM_MBUTTONDOWN: return ps_mswm_evt_mbtn(2,1);
    case WM_MBUTTONUP: return ps_mswm_evt_mbtn(2,0);
    case WM_RBUTTONDOWN: return ps_mswm_evt_mbtn(3,1);
    case WM_RBUTTONUP: return ps_mswm_evt_mbtn(3,0);

    // WM_MOUSEHWHEEL exists but my 2-wheel mouse doesn't report it. In fact there is no difference between the two wheels.
    case WM_MOUSEHWHEEL: return ps_mswm_evt_mwheel(wparam,lparam,1);
    case WM_MOUSEWHEEL: return ps_mswm_evt_mwheel(wparam,lparam,0);

    case WM_MOUSEMOVE: return ps_mswm_evt_mmove(LOWORD(lparam),HIWORD(lparam));

    case WM_INPUT: return ps_mshid_event(wparam,lparam);
    case WM_DEVICECHANGE: {
        if (wparam==7) { // DBT_DEVNODES_CHANGED
          if (ps_mshid_poll_disconnected_devices()<0) return -1;
        }
      } break;

    case WM_SETCURSOR: if (LOWORD(lparam)==HTCLIENT) {
        //ps_log(MSWM,DEBUG,"WM_SETCURSOR with HTCLIENT");
        SetCursor(0);
        return 1;
      } else {
        //ps_log(MSWM,DEBUG,"WM_SETCURSOR with other lparam");
        ShowCursor(1);
      } break;

    //default: ps_log(MSWM,TRACE,"event %d",msg);

  }
  return DefWindowProc(hwnd,msg,wparam,lparam);
}

/* Update.
 */
 
int ps_mswm_update(struct ps_input_provider *dummy) {
  if (!ps_mswm.init) return -1;
  int msgc=0;
  MSG msg={0};
  while (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
    if (!msg.hwnd) {
      ps_mswm_cb_msg(msg.hwnd,msg.message,msg.wParam,msg.lParam);
    } else {
      if (ps_mswm.translate_events) {
        TranslateMessage(&msg);
      }
      DispatchMessage(&msg);
    }
    msgc++;
  }
  return msgc;
}
