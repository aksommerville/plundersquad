#include "ps_mswm_internal.h"

struct ps_mswm ps_mswm={0};

/* Initialize input provider.
 */

int ps_mswm_connect_input() {
  if (!ps_mswm.init) return -1;
  if (ps_mswm.input_provider) return -1;
  
  if (!(ps_mswm.input_provider=ps_input_provider_new(0))) return -1;
  ps_mswm.input_provider->providerid=PS_INPUT_PROVIDER_mswm;
  ps_mswm.input_provider->update=ps_mswm_update;
  if (ps_input_install_provider(ps_mswm.input_provider)<0) return -1;

  if (!(ps_mswm.dev_keyboard=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_mswm.dev_keyboard,"Keyboard",-1)<0) return -1;
  ps_mswm.dev_keyboard->report_buttons=ps_mswm_report_buttons_keyboard;
  if (ps_input_provider_install_device(ps_mswm.input_provider,ps_mswm.dev_keyboard)<0) return -1;
  if (ps_input_event_connect(ps_mswm.dev_keyboard)<0) return -1;
  
  return 0;
}

/* Setup window class.
 */

static int ps_mswm_populate_wndclass() {
  ps_mswm.wndclass.cbSize=sizeof(WNDCLASSEX);
  ps_mswm.wndclass.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  ps_mswm.wndclass.lpfnWndProc=(WNDPROC)ps_mswm_cb_msg;
  ps_mswm.wndclass.hInstance=ps_mswm.instance;
  ps_mswm.wndclass.hCursor=LoadCursor(0,IDC_ARROW);
  ps_mswm.wndclass.lpszClassName=PS_MSWM_WINDOW_CLASS_NAME;
  return 0;
}

static int ps_mswm_register_wndclass() {
  ps_mswm.wndclass_atom=RegisterClassEx(&ps_mswm.wndclass);
  if (!ps_mswm.wndclass_atom) {
    int errcode=GetLastError();
    ps_log(MSWM,ERROR,"RegisterClassEx(\"%s\"): Error %d",PS_MSWM_WINDOW_CLASS_NAME,errcode);
    return -1;
  }
  return 0;
}

/* Init.
 */

int ps_mswm_init(int w,int h,int fullscreen,const char *title) {
  if (ps_mswm.init) return -1;
  ps_mswm.init=1;
  if (!title) title="";
  
  ps_mswm.translate_events=1;

  if (!(ps_mswm.instance=GetModuleHandle(0))) return -1;

  if (ps_mswm_populate_wndclass()<0) return -1;
  if (ps_mswm_register_wndclass()<0) return -1;

  ps_mswm.winw=w;
  ps_mswm.winh=h;
  int x=100,y=100;//TODO window position

  ps_mswm.hwnd=CreateWindow(
    PS_MSWM_WINDOW_CLASS_NAME,
    title,
    WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
    x,y,w,h,
    0,0,ps_mswm.instance,0
  );
  if (!ps_mswm.hwnd) {
    ps_log(MSWM,ERROR,"Failed to create window.");
    ps_mswm_quit();
    return -1;
  }

  ShowWindow(ps_mswm.hwnd,SW_SHOW);
  UpdateWindow(ps_mswm.hwnd);

  return 0;
}

/* Quit.
 */

void ps_mswm_quit() {
  if (!ps_mswm.init) return;

  ps_input_device_del(ps_mswm.dev_keyboard);
  ps_input_uninstall_provider(ps_mswm.input_provider);
  ps_input_provider_del(ps_mswm.input_provider);
  
  if (ps_mswm.hglrc) {
    wglMakeCurrent(ps_mswm.hdc,0);
    wglDeleteContext(ps_mswm.hglrc);
  }
  if (ps_mswm.hdc) {
    DeleteDC(ps_mswm.hdc);
  }
  if (ps_mswm.hwnd) {
    DestroyWindow(ps_mswm.hwnd);
  }

  if (ps_mswm.wndclass_atom) {
    UnregisterClass(PS_MSWM_WINDOW_CLASS_NAME,ps_mswm.instance);
  }

  memset(&ps_mswm,0,sizeof(struct ps_mswm));
}
