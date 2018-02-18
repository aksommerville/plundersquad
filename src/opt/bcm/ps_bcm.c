#include <stdarg.h>
#include "ps_bcm.h"
#include "video/ps_video.h"
#include <bcm_host.h>
#include <EGL/egl.h>

/* Globals.
 */

static struct {
  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  EGL_DISPMANX_WINDOW_T eglwindow;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLConfig eglconfig;
  int initstate;
  int screenw,screenh;
} ps_bcm={0};

/* Init.
 */

int ps_bcm_init() {
  if (ps_bcm.initstate) return -1;
  memset(&ps_bcm,0,sizeof(ps_bcm));

  bcm_host_init();
  ps_bcm.initstate=1;

  graphics_get_display_size(0,&ps_bcm.screenw,&ps_bcm.screenh);
  if ((ps_bcm.screenw<1)||(ps_bcm.screenh<1)) return -1;
  if ((ps_bcm.screenw>4096)||(ps_bcm.screenh>4096)) return -1;
  //printf("SCREEN %d,%d\n",ps_bcm.screenw,ps_bcm.screenh);

  if (!(ps_bcm.vcdisplay=vc_dispmanx_display_open(0))) return -1;
  if (!(ps_bcm.vcupdate=vc_dispmanx_update_start(0))) return -1;

  VC_RECT_T srcr={0,0,ps_bcm.screenw<<16,ps_bcm.screenh<<16};
  VC_RECT_T dstr={0,0,ps_bcm.screenw,ps_bcm.screenh};
  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(ps_bcm.vcelement=vc_dispmanx_element_add(
    ps_bcm.vcupdate,ps_bcm.vcdisplay,1,&dstr,0,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) { ps_bcm_quit(); return -1; }

  ps_bcm.eglwindow.element=ps_bcm.vcelement;
  ps_bcm.eglwindow.width=ps_bcm.screenw;
  ps_bcm.eglwindow.height=ps_bcm.screenh;

  if (vc_dispmanx_update_submit_sync(ps_bcm.vcupdate)<0) return -1;

  static const EGLint eglattr[]={
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,0,
    EGL_DEPTH_SIZE,0,
    EGL_LUMINANCE_SIZE,EGL_DONT_CARE,
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_SAMPLES,1,
  EGL_NONE};
  static EGLint ctxattr[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
  EGL_NONE};

  ps_bcm.egldisplay=eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglInitialize(ps_bcm.egldisplay,0,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;
  ps_bcm.initstate=2;

  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint configc=0;
  eglChooseConfig(ps_bcm.egldisplay,eglattr,&ps_bcm.eglconfig,1,&configc);
  if (eglGetError()!=EGL_SUCCESS) return -1;
  if (configc<1) return -1;

  ps_bcm.eglsurface=eglCreateWindowSurface(ps_bcm.egldisplay,ps_bcm.eglconfig,&ps_bcm.eglwindow,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;
  ps_bcm.initstate=3;

  ps_bcm.eglcontext=eglCreateContext(ps_bcm.egldisplay,ps_bcm.eglconfig,0,ctxattr);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglMakeCurrent(ps_bcm.egldisplay,ps_bcm.eglsurface,ps_bcm.eglsurface,ps_bcm.eglcontext);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglSwapInterval(ps_bcm.egldisplay,1);
  
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE);

  if (ps_video_resized(ps_bcm.screenw,ps_bcm.screenh)<0) return -1;

  return 0;
}

/* Quit.
 */

void ps_bcm_quit() {
  if (ps_bcm.initstate>=3) {
    eglMakeCurrent(ps_bcm.egldisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(ps_bcm.egldisplay,ps_bcm.eglsurface);
  }
  if (ps_bcm.initstate>=2) {
    eglTerminate(ps_bcm.egldisplay);
    eglReleaseThread();
  }
  if (ps_bcm.initstate>=1) bcm_host_deinit();
  memset(&ps_bcm,0,sizeof(ps_bcm));
}

/* Swap buffers.
 */

int ps_bcm_swap() {
  if (!ps_bcm.initstate) return -1;
  eglSwapBuffers(ps_bcm.egldisplay,ps_bcm.eglsurface);
  return 0;
}
