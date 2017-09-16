#ifndef PS_MACWM_INTERNAL_H
#define PS_MACWM_INTERNAL_H

#include "ps.h"
#include "ps_macwm.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include "input/ps_input_button.h"
#include "video/ps_video.h"
#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#ifndef GL_PROGRAM_POINT_SIZE
  #ifdef GL_PROGRAM_POINT_SIZE_EXT
    #define GL_PROGRAM_POINT_SIZE GL_PROGRAM_POINT_SIZE_EXT
  #elif defined(GL_VERTEX_PROGRAM_POINT_SIZE)
    #define GL_PROGRAM_POINT_SIZE GL_VERTEX_PROGRAM_POINT_SIZE
  #endif
#endif

/* Declaration of window class.
 */

@interface PsWindow : NSWindow <NSWindowDelegate> {
@public
  NSOpenGLContext *context;
  int cursor_visible;
  int w,h;
  int mousex,mousey;
}

+(PsWindow*)newWithWidth:(int)width 
  height:(int)height 
  title:(const char*)title
  fullscreen:(int)fullscreen
;

-(int)beginFrame;
-(int)endFrame;

@end

/* Globals.
 */

extern struct ps_macwm {
  PsWindow *window;
  struct ps_input_provider *input_provider;
  struct ps_input_device *device_wm;
  struct ps_input_device *device_keyboard;
  struct ps_input_device *device_mouse;
} ps_macwm;

/* Miscellaneous.
 */

void ps_macwm_abort(const char *fmt,...);

int ps_macwm_decode_utf8(int *dst,const void *src,int srcc);
int ps_macwm_translate_codepoint(int src);
int ps_macwm_translate_keysym(int src);
int ps_macwm_translate_mbtn(int src);

int ps_macwm_btnid_repr(char *dst,int dsta,int btnid);
int ps_macwm_btnid_eval(int *btnid,const char *src,int srcc);
int ps_macwm_report_buttons_wm(struct ps_input_device *device,void *userdata,int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata));
int ps_macwm_report_buttons_keyboard(struct ps_input_device *device,void *userdata,int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata));
int ps_macwm_report_buttons_mouse(struct ps_input_device *device,void *userdata,int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata));

#endif
