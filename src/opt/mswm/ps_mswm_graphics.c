#include "ps_mswm_internal.h"

/* Setup graphics context.
 * Called when the new window's WM_CREATE triggers.
 */

/* Assert necessary OpenGL features.
 */

static int ps_mswm_assert_opengl() {

  /* Best-effort list to capture all missing requirements. */
  #define reqa 16
  const char *reqv[reqa];
  int reqc=0,totalc=0;
  
  #define REQUIRE(name) { \
    totalc++; \
    if (!glewIsSupported(name)) { \
      if (reqc<reqa) reqv[reqc]=name; \
      reqc++; \
    } \
  }

  //TODO Assert OpenGL features.
  // Try to put the likeliest to fail higher, since the list of failed requirements is limited.
  REQUIRE("GL_VERSION_2_0")
  REQUIRE("GL_EXT_framebuffer_object")
  REQUIRE("GL_ARB_fragment_shader")
  REQUIRE("GL_ARB_vertex_shader")
  REQUIRE("GL_ARB_point_sprite")

  if (reqc) {
    ps_log(MSWM,ERROR,"%d of %d required OpenGL features are not supported:",reqc,totalc);
    int listc=(reqc<reqa)?reqc:reqa;
    int i; for (i=0;i<listc;i++) {
      ps_log(MSWM,ERROR,"  %s",reqv[i]);
    }
    if (reqc>reqa) ps_log(MSWM,ERROR,"...%d more omitted for brevity.",reqc-reqa);
    return -1;
  }

  #undef REQUIRE
  #undef reqa
  return reqc?-1:0;
}

/* Window is created; finish composing its graphics context.
 */

int ps_mswm_setup_window() {
  ps_log(MSWM,TRACE,"ps_mswm_setup_window");

  if (ps_mswm.window_setup_complete) return -1;
  ps_mswm.window_setup_complete=1;
  
  if (!(ps_mswm.hdc=GetDC(ps_mswm.hwnd))) return -1;

  /* Select pixel format. */
  PIXELFORMATDESCRIPTOR pixfmt={
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    32,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,
    16,
  };
  int fmtix=ChoosePixelFormat(ps_mswm.hdc,&pixfmt);
  if (fmtix<0) return -1;
  SetPixelFormat(ps_mswm.hdc,fmtix,&pixfmt);

  /* Initialize OpenGL. */
  if (!(ps_mswm.hglrc=wglCreateContext(ps_mswm.hdc))) {
    ps_log(MSWM,ERROR,"Failed to create OpenGL context.");
    return -1;
  }   
  wglMakeCurrent(ps_mswm.hdc,ps_mswm.hglrc);

  if (glewInit()) return -1;
  if (ps_mswm_assert_opengl()<0) return -1;

  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE);
  //glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  return 0;
}

/* Swap frames.
 */

int ps_mswm_swap() {
  if (!ps_mswm.hdc) return 0;
  SwapBuffers(ps_mswm.hdc);
  return 0;
}
