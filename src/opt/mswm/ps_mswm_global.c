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

  /* CreateWindow() takes the outer bounds, including the frame.
   * (w,h) provided to us are the desired inner bounds, ie the part that we control.
   */
  RECT bounds={
    .left=0,
    .top=0,
    .right=w,
    .bottom=h,
  };
  AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0);
  int outerw=bounds.right-bounds.left;
  int outerh=bounds.bottom-bounds.top;

  /* If possible, center in the primary monitor.
   * Experimentally, it seems the program always launches on primary, which is fine by me.
   */
  ps_mswm.winw=w;
  ps_mswm.winh=h;
  int x=0,y=0;
  POINT pt={.x=INT_MIN,.y=INT_MIN}; // Choose a point sure to be out of bounds.
  HMONITOR monitor=MonitorFromPoint(pt,MONITOR_DEFAULTTOPRIMARY);
  if (monitor) {
    MONITORINFO mi={.cbSize=sizeof(MONITORINFO)};
    GetMonitorInfo(monitor,&mi);
    RECT r=mi.rcMonitor;
    x=r.left+((r.right-r.left)>>1)-(w>>1);
    y=r.top+((r.bottom-r.top)>>1)-(h>>1);
  }

  ps_mswm.hwnd=CreateWindow(
    PS_MSWM_WINDOW_CLASS_NAME,
    title,
    fullscreen?(WS_POPUP):(WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS),
    x,y,outerw,outerh,
    0,0,ps_mswm.instance,0
  );
  if (!ps_mswm.hwnd) {
    ps_log(MSWM,ERROR,"Failed to create window.");
    ps_mswm_quit();
    return -1;
  }

  if (fullscreen) {
    ps_mswm.fullscreen=1;
    ShowWindow(ps_mswm.hwnd,SW_MAXIMIZE);
  } else {
    ShowWindow(ps_mswm.hwnd,SW_SHOW);
  }
  UpdateWindow(ps_mswm.hwnd);

  ShowCursor(0);

  return 0;
}

/* Quit.
 */

void ps_mswm_quit() {
  if (!ps_mswm.init) return;

  ps_input_device_del(ps_mswm.dev_keyboard);
  ps_input_uninstall_provider(ps_mswm.input_provider);
  ps_input_provider_del(ps_mswm.input_provider);

  if (ps_mswm.appicon) DestroyIcon(ps_mswm.appicon);

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

/* Set fullscreen.
 */

static int ps_mswm_enter_fullscreen() {
  ps_mswm.fsrestore.length=sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(ps_mswm.hwnd,&ps_mswm.fsrestore);
  SetWindowLong(ps_mswm.hwnd,GWL_STYLE,WS_POPUP);
  ShowWindow(ps_mswm.hwnd,SW_MAXIMIZE);
  ps_mswm.fullscreen=1;
  return 0;
}

static int ps_mswm_exit_fullscreen() {
  SetWindowLong(ps_mswm.hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);

  /* If we started up in fullscreen mode, we don't know the appropriate size to restore to.
   * Make something up.
   * TODO: Other windows don't get repainted when we resize. Giving up on that for now.
   */
  if (!ps_mswm.fsrestore.length) {
    ps_mswm.fsrestore.length=sizeof(WINDOWPLACEMENT);
    ps_mswm.fsrestore.flags=WPF_SETMINPOSITION;
    ps_mswm.fsrestore.showCmd=SW_SHOW;
    ps_mswm.fsrestore.rcNormalPosition.left=100;
    ps_mswm.fsrestore.rcNormalPosition.top=100;
    ps_mswm.fsrestore.rcNormalPosition.right=100+(PS_SCREENW*2);
    ps_mswm.fsrestore.rcNormalPosition.bottom=100+(PS_SCREENH*2);
    AdjustWindowRect(&ps_mswm.fsrestore.rcNormalPosition,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0);
  }
  
  SetWindowPlacement(ps_mswm.hwnd,&ps_mswm.fsrestore);
  SetWindowPos(ps_mswm.hwnd,0,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_FRAMECHANGED);
  ps_mswm.fullscreen=0;
  return 0;
}
 
int ps_mswm_set_fullscreen(int flag) {
  if (!ps_mswm.window_setup_complete) return -1;

  if (flag>0) {
    if (ps_mswm.fullscreen) return 1;
    if (ps_mswm_enter_fullscreen()<0) return -1;
    return 1;

  } else if (!flag) {
    if (!ps_mswm.fullscreen) return 0;
    if (ps_mswm_exit_fullscreen()<0) return -1;
    return 0;

  } else if (ps_mswm.fullscreen) {
    if (ps_mswm_exit_fullscreen()<0) return -1;
    return 0;

  } else {
    if (ps_mswm_enter_fullscreen()<0) return -1;
    return 1;
  }
}

/* Show cursor.
 */
 
int ps_mswm_show_cursor(int flag) {
  if (!ps_mswm.init) return -1;
  if (flag) {
    if (ps_mswm.cursor) return 0;
    //ShowCursor(1);
    ps_mswm.showcursor=1;
  } else{
    if (!ps_mswm.cursor) return 0;
    //ShowCursor(0);
    ps_mswm.showcursor=0;
  }
  return 0;
}

/* Set icon.
 */

static void ps_mswm_copy_icon(uint8_t *dst,const uint8_t *src,int c) {
  uint8_t maskbit=0x01;
  for (;c-->0;dst+=4,src+=4) {
    dst[0]=src[2];
    dst[1]=src[1];
    dst[2]=src[0];
    dst[3]=src[3];;
  }
}

int ps_mswm_set_icon(const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>1024)||(h>1024)) return -1;
  if (ps_mswm.appicon) DestroyIcon(ps_mswm.appicon);

  void *dst=malloc(w*h*4);
  if (!dst) return -1;
  ps_mswm_copy_icon(dst,rgba,w*h);
  
  if (!(ps_mswm.appicon=CreateIcon(0,w,h,1,32,0,dst))) {
    free(dst);
    return -1;
  }
  free(dst);
  
  SetClassLongPtr(ps_mswm.hwnd,GCLP_HICON,(LONG)ps_mswm.appicon);

  return 0;
}
