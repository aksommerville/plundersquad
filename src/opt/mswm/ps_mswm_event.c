#include "ps_mswm_internal.h"

/* Receive event.
 */

LRESULT ps_mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) {
  ps_log(MSWM,TRACE,"%s %d %d %d",__func__,msg,wparam,lparam);
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

    #if 0//TODO copied from skeleton/wingdi
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
        skeleton_log_debug("WM_CHAR U+%x",wparam);//TODO input
      } return 0;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: break;
    case WM_KEYDOWN:
    case WM_KEYUP: {
        return ps_mswm_evt_key(wparam,lparam);
      }

    case WM_LBUTTONDOWN: return ps_mswm_evt_mbtn(SKELETON_MBTN_LEFT,1);
    case WM_LBUTTONUP: return ps_mswm_evt_mbtn(SKELETON_MBTN_LEFT,0);
    case WM_MBUTTONDOWN: return ps_mswm_evt_mbtn(SKELETON_MBTN_MIDDLE,1);
    case WM_MBUTTONUP: return ps_mswm_evt_mbtn(SKELETON_MBTN_MIDDLE,0);
    case WM_RBUTTONDOWN: return ps_mswm_evt_mbtn(SKELETON_MBTN_RIGHT,1);
    case WM_RBUTTONUP: return ps_mswm_evt_mbtn(SKELETON_MBTN_RIGHT,0);

    case WM_MOUSEHWHEEL: return ps_mswm_evt_mwheel(((signed int)wparam)>>16,0);
    case WM_MOUSEWHEEL: return ps_mswm_evt_mwheel(0,((signed int)wparam)>>16);

    case WM_MOUSEMOVE: return ps_mswm_evt_mmove(LOWORD(lparam),HIWORD(lparam));

    case WM_INPUT: return ps_mswm_raw_input(wparam,lparam);
    #endif

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
