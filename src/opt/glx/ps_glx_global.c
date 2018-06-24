#include "ps_glx_internal.h"

/* Globals.
 */
 
struct ps_glx ps_glx={0};
 
/* Initialize X11 (display is already open).
 */
 
static int ps_glx_startup(int w,int h,int fullscreen,const char *title) {
  
  #define GETATOM(tag) ps_glx.atom_##tag=XInternAtom(ps_glx.dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  #undef GETATOM

  int attrv[]={
    GLX_X_RENDERABLE,1,
    GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_ALPHA_SIZE,0,
    GLX_DEPTH_SIZE,0,
    GLX_STENCIL_SIZE,0,
    GLX_DOUBLEBUFFER,1,
  0};
  
  if (!glXQueryVersion(ps_glx.dpy,&ps_glx.glx_version_major,&ps_glx.glx_version_minor)) {
    ps_log(GLX,ERROR,"Failed to acquire GLX version.");
    return -1;
  }
  ps_log(GLX,INFO,"GLX version %d.%d.",ps_glx.glx_version_major,ps_glx.glx_version_minor);
  
  int fbc=0;
  GLXFBConfig *configv=glXChooseFBConfig(ps_glx.dpy,ps_glx.screen,attrv,&fbc);
  if (!configv||(fbc<1)) {
    ps_log(GLX,ERROR,"Failed to acquire GLX framebuffer configuration.");
    return -1;
  }
  GLXFBConfig config=configv[0];
  XFree(configv);
  
  XVisualInfo *vi=glXGetVisualFromFBConfig(ps_glx.dpy,config);
  if (!vi) {
    ps_log(GLX,ERROR,"Failed to acquire visual info.");
    return -1;
  }
  
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      ButtonPressMask|ButtonReleaseMask|
      PointerMotionMask|
    0,
  };
  wattr.colormap=XCreateColormap(ps_glx.dpy,RootWindow(ps_glx.dpy,vi->screen),vi->visual,AllocNone);
  
  if (!(ps_glx.win=XCreateWindow(
    ps_glx.dpy,RootWindow(ps_glx.dpy,vi->screen),
    0,0,w,h,0,
    vi->depth,InputOutput,vi->visual,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  ))) {
    ps_log(GLX,ERROR,"Failed to create X11 window.");
    XFree(vi);
    return -1;
  }
  
  XFree(vi);
  
  if (title) {
    XStoreName(ps_glx.dpy,ps_glx.win,title);
  }
  
  if (fullscreen) {
    XChangeProperty(ps_glx.dpy,ps_glx.win,ps_glx.atom__NET_WM_STATE,XA_ATOM,32,PropModeReplace,(unsigned char*)&ps_glx.atom__NET_WM_STATE_FULLSCREEN,1);
    ps_glx.fullscreen=1;
  }
  
  XMapWindow(ps_glx.dpy,ps_glx.win);
  
  if (!(ps_glx.ctx=glXCreateNewContext(ps_glx.dpy,config,GLX_RGBA_TYPE,0,1))) {
    ps_log(GLX,ERROR,"Failed to acquire GLX context.");
    return -1;
  }
  glXMakeCurrent(ps_glx.dpy,ps_glx.win,ps_glx.ctx);
  
  XSync(ps_glx.dpy,0);
  
  ps_log(GLX,INFO,"glXIsDirect = %d",glXIsDirect(ps_glx.dpy,ps_glx.ctx));
  
  XSetWMProtocols(ps_glx.dpy,ps_glx.win,&ps_glx.atom_WM_DELETE_WINDOW,1);

  return 0;
}
 
/* Init.
 */
 
int ps_glx_init(int w,int h,int fullscreen,const char *title) {
  ps_log(GLX,TRACE,"%s",__func__);
  if (ps_glx.dpy) return -1;
  
  ps_glx.w=w;
  ps_glx.h=h;
  ps_glx.cursor_visible=1;
  
  if (!(ps_glx.dpy=XOpenDisplay(0))) {
    ps_log(GLX,ERROR,"Failed to open X11 display.");
    return -1;
  }
  ps_glx.screen=DefaultScreen(ps_glx.dpy);
  
  if (ps_glx_startup(w,h,fullscreen,title)<0) {
    ps_log(GLX,ERROR,"Failed to initialize X11/GLX.");
    return -1;
  }
  
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE);

  return 0;
}

/* Quit.
 */

void ps_glx_quit() {
  ps_log(GLX,TRACE,"%s",__func__);

  ps_input_uninstall_provider(ps_glx.input_provider);
  ps_input_device_del(ps_glx.dev_keyboard);
  ps_input_device_del(ps_glx.dev_mouse);
  ps_input_provider_del(ps_glx.input_provider);
  
  if (ps_glx.ctx) {
    glXMakeCurrent(ps_glx.dpy,0,0);
    glXDestroyContext(ps_glx.dpy,ps_glx.ctx);
  }
  if (ps_glx.dpy) {
    XCloseDisplay(ps_glx.dpy);
  }
  
}

/* Frame control.
 */

int ps_glx_swap() {
  //ps_log(GLX,TRACE,"%s",__func__);
  if (!ps_glx.dpy) return -1;
  glXSwapBuffers(ps_glx.dpy,ps_glx.win);
  ps_glx.screensaver_inhibited=0;
  return 0;
}

/* Create input provider and register with plundersquad input manager.
 */
 
int ps_glx_connect_input() {
  if (!ps_glx.dpy) return -1;
  if (ps_glx.input_provider) return 0;
  
  if (!(ps_glx.input_provider=ps_input_provider_new(0))) return -1;
  ps_glx.input_provider->providerid=PS_INPUT_PROVIDER_x11;
  ps_glx.input_provider->update=ps_glx_update;
  if (ps_input_install_provider(ps_glx.input_provider)<0) return -1;
  
  if (!(ps_glx.dev_keyboard=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_glx.dev_keyboard,"Keyboard",-1)<0) return -1;
  ps_glx.dev_keyboard->report_buttons=ps_glx_report_buttons_keyboard;
  if (ps_input_provider_install_device(ps_glx.input_provider,ps_glx.dev_keyboard)<0) return -1;
  if (ps_input_event_connect(ps_glx.dev_keyboard)<0) return -1;
  
  if (!(ps_glx.dev_mouse=ps_input_device_new(0))) return -1;
  if (ps_input_device_set_name(ps_glx.dev_mouse,"Mouse",-1)<0) return -1;
  ps_glx.dev_mouse->report_buttons=ps_glx_report_buttons_mouse;
  if (ps_input_provider_install_device(ps_glx.input_provider,ps_glx.dev_mouse)<0) return -1;
  if (ps_input_event_connect(ps_glx.dev_mouse)<0) return -1;
  
  return 0;
}

/* Toggle fullscreen.
 */

static int ps_glx_enter_fullscreen() {
  ps_log(GLX,TRACE,"%s",__func__);
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=ps_glx.atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=ps_glx.win,
      .data={.l={
        1,
        ps_glx.atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(ps_glx.dpy,RootWindow(ps_glx.dpy,ps_glx.screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(ps_glx.dpy);
  ps_glx.fullscreen=1;
  return 0;
}

static int ps_glx_exit_fullscreen() {
  ps_log(GLX,TRACE,"%s",__func__);
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=ps_glx.atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=ps_glx.win,
      .data={.l={
        0,
        ps_glx.atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(ps_glx.dpy,RootWindow(ps_glx.dpy,ps_glx.screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(ps_glx.dpy);
  ps_glx.fullscreen=0;
  return 0;
}
 
int ps_glx_set_fullscreen(int state) {
  if (!ps_glx.dpy) return -1;
  
  if (state>0) {
    if (ps_glx.fullscreen) return 1;
    if (ps_glx_enter_fullscreen()<0) return -1;
    return 1;

  } else if (state==0) {
    if (!ps_glx.fullscreen) return 0;
    if (ps_glx_exit_fullscreen()<0) return -1;
    return 0;

  } else if (ps_glx.fullscreen) {
    if (ps_glx_exit_fullscreen()<0) return -1;
    return 0;

  } else {
    if (ps_glx_enter_fullscreen()<0) return -1;
    return 1;
  }
}

/* Cursor visibility.
 */

static int ps_glx_set_cursor_invisible() {
  XColor color;
  Pixmap pixmap=XCreateBitmapFromData(ps_glx.dpy,ps_glx.win,"\0\0\0\0\0\0\0\0",1,1);
  Cursor cursor=XCreatePixmapCursor(ps_glx.dpy,pixmap,pixmap,&color,&color,0,0);
  XDefineCursor(ps_glx.dpy,ps_glx.win,cursor);
  XFreeCursor(ps_glx.dpy,cursor);
  XFreePixmap(ps_glx.dpy,pixmap);
  return 0;
}

static int ps_glx_set_cursor_visible() {
  XDefineCursor(ps_glx.dpy,ps_glx.win,None);
  return 0;
}

int ps_glx_show_cursor(int show) {
  if (!ps_glx.dpy) return -1;
  ps_log(GLX,TRACE,"%s(%d) current=%d",__func__,show,ps_glx.cursor_visible);
  if (show) {
    if (ps_glx.cursor_visible) return 0;
    if (ps_glx_set_cursor_visible()<0) return -1;
    ps_glx.cursor_visible=1;
  } else {
    if (!ps_glx.cursor_visible) return 0;
    if (ps_glx_set_cursor_invisible()<0) return -1;
    ps_glx.cursor_visible=0;
  }
  return 0;
}

/* Set window icon.
 */
 
static void ps_glx_copy_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    #if BYTE_ORDER==BIG_ENDIAN
      /* https://standards.freedesktop.org/wm-spec/wm-spec-1.3.html
       * """
       * This is an array of 32bit packed CARDINAL ARGB with high byte being A, low byte being B.
       * The first two cardinals are width, height. Data is in rows, left to right and top to bottom.
       * """
       * I take this to mean that big-endian should work the same as little-endian.
       * But I'm nervous about it because:
       *  - I don't have any big-endian machines handy for testing.
       *  - The data type is "long" which is not always "32bit" as they say. (eg it is 64 on my box)
       */
      *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
    #else
      *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
    #endif
  }
}
 
int ps_glx_set_icon(const void *rgba,int w,int h) {
  if (!ps_glx.dpy) return -1;
  if (!rgba||(w<1)||(h<1)) return -1;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return -1;
  pixels[0]=w;
  pixels[1]=h;
  ps_glx_copy_pixels(pixels+2,rgba,w*h);
  XChangeProperty(ps_glx.dpy,ps_glx.win,ps_glx.atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
  return 0;
}

/* Inhibit screensaver.
 */
 
int ps_glx_inhibit_screensaver() {
  if (!ps_glx.dpy) return -1;
  if (ps_glx.screensaver_inhibited) return 0;
  XForceScreenSaver(ps_glx.dpy,ScreenSaverReset);
  ps_glx.screensaver_inhibited=1;
  return 0;
}
