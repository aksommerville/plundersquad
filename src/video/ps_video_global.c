#include "ps_video_internal.h"
#include "os/ps_ioc.h"

struct ps_video ps_video={0};

/* Application icon, for providers that support such a thing.
 */

extern const int ps_appicon_w;
extern const int ps_appicon_h;
extern const char ps_appicon[];

/* Initialize akgl and the associated objects.
 */

static int ps_video_init_akgl(int strategy) {

  if (akgl_init(strategy)<0) return -1;

  if (akgl_set_screen_size(ps_video.winw,ps_video.winh)<0) return -1;
  if (akgl_set_glsl_version(PS_GLSL_VERSION)<0) return -1;
  
  if (!(ps_video.framebuffer=akgl_framebuffer_new())) return -1;
  if (akgl_framebuffer_resize(ps_video.framebuffer,PS_SCREENW,PS_SCREENH)<0) return -1;

  switch (strategy) {

    case AKGL_STRATEGY_SOFT: {
      } break;

    case AKGL_STRATEGY_GL2: {
        // In hindsight, ownership of program objects should have been akgl's, not the client's.
        if (!(ps_video.program_raw=akgl_program_raw_new())) return -1;
        if (!(ps_video.program_fbxfer=akgl_program_fbxfer_new())) return -1;
        if (!(ps_video.program_tex=akgl_program_tex_new())) return -1;
        if (!(ps_video.program_mintile=akgl_program_mintile_new())) return -1;
        if (!(ps_video.program_maxtile=akgl_program_maxtile_new())) return -1;
        if (!(ps_video.program_textile=akgl_program_textile_new())) return -1;
      } break;

    default: return -1;
  }

  if (!(ps_video.texture_minfont=akgl_texture_new())) return -1;
  if (ps_video_load_minfont(ps_video.texture_minfont)<0) return -1;

  return 0;
}

/* Init.
 */

int ps_video_init(const struct ps_cmdline *cmdline) {

  if (ps_video.init) return -1;
  if (!cmdline) return -1;
  memset(&ps_video,0,sizeof(struct ps_video));
  ps_video.init=1;

  ps_video.winw=PS_SCREENW<<1;
  ps_video.winh=PS_SCREENH<<1;
  ps_video.dstx=0;
  ps_video.dsty=0;
  ps_video.dstw=ps_video.winw;
  ps_video.dsth=ps_video.winh;

  #if PS_USE_macwm
    if (ps_macwm_init(ps_video.winw,ps_video.winh,cmdline->fullscreen,"Plunder Squad")<0) {
      ps_log(VIDEO,ERROR,"Failed to create MacOS window.");
      ps_video_quit();
      return -1;
    }

  #elif PS_USE_bcm
    if (ps_bcm_init()<0) {
      ps_log(VIDEO,ERROR,"Failed to initialize Broadcom video.");
      ps_video_quit();
      return -1;
    }
    
  #elif PS_USE_glx
    if (ps_glx_init(ps_video.winw,ps_video.winh,cmdline->fullscreen,"Plunder Squad")<0) {
      ps_log(VIDEO,ERROR,"Failed to initialize X11/GLX video.");
      ps_video_quit();
      return -1;
    }
    if (ps_glx_show_cursor(0)<0) {
      ps_log(VIDEO,ERROR,"Failed to hide cursor. Ignoring error.");
    }
    if (ps_glx_set_icon(ps_appicon,ps_appicon_w,ps_appicon_h)<0) {
      ps_log(VIDEO,ERROR,"Failed to set window icon. Ignoring error.");
    }

  #elif PS_USE_mswm
    if (ps_mswm_init(ps_video.winw,ps_video.winh,cmdline->fullscreen,"Plunder Squad")<0) {
      ps_log(VIDEO,ERROR,"Failed to create window.");
      ps_video_quit();
      return -1;
    }
  #endif

  if (ps_video_init_akgl(cmdline->akgl_strategy)<0) {
    ps_log(VIDEO,ERROR,"Failed to initialize OpenGL.");
    ps_video_quit();
    return -1;
  }

  return 0;
}

/* Quit.
 */

void ps_video_quit() {

  if (ps_video.layerv) {
    while (ps_video.layerc-->0) {
      ps_video_layer_del(ps_video.layerv[ps_video.layerc]);
    }
    free(ps_video.layerv);
  }

  akgl_program_del(ps_video.program_raw);
  akgl_program_del(ps_video.program_tex);
  akgl_program_del(ps_video.program_fbxfer);
  akgl_program_del(ps_video.program_mintile);
  akgl_program_del(ps_video.program_maxtile);
  akgl_program_del(ps_video.program_textile);
  akgl_texture_del(ps_video.texture_minfont);
  akgl_framebuffer_del(ps_video.framebuffer);
  akgl_quit();

  #if PS_USE_macwm
    ps_macwm_quit();
  #elif PS_USE_bcm
    ps_bcm_quit();
  #elif PS_USE_glx
    ps_glx_quit();
  #elif PS_USE_mswm
    ps_mswm_quit();
  #endif

  if (ps_video.vtxv) free(ps_video.vtxv);

  memset(&ps_video,0,sizeof(struct ps_video));
}

/* Trivial accessors.
 */

int ps_video_is_init() {
  return ps_video.init;
}

/* Draw layers.
 */

static int ps_video_draw_layers(int game_only) {

  int must_clear=1,startp=0,i;
  for (i=ps_video.layerc;i-->0;) {
    struct ps_video_layer *layer=ps_video.layerv[i];
    if (layer->blackout) {
      must_clear=0;
      startp=i;
      break;
    }
  }

  if (must_clear) {
    if (akgl_clear(0x000000ff)<0) return -1;
  }

  for (i=startp;i<ps_video.layerc;i++) {
    struct ps_video_layer *layer=ps_video.layerv[i];
    if (!layer->draw) continue;
    if (layer->draw(layer)<0) return -1;
    if (game_only) break; // Foolishly assume that the bottom layer is the game.
  }

  return 0;
}

/* Update.
 */

int ps_video_update() {
  if (!ps_video.init) return -1;

  /* Draw scene into the framebuffer. */
  if (akgl_framebuffer_use(ps_video.framebuffer)<0) return -1;
  
  if (ps_video_draw_layers(0)<0) return -1;
  
  if (akgl_framebuffer_use(0)<0) return -1;
  
  /* If the output bounds don't cover the window, black out first. */
  if (ps_video.dstx||ps_video.dsty||(ps_video.dstw<ps_video.winw)||(ps_video.dsth<ps_video.winh)) {
    akgl_clear(0x000000ff);
  }

  if (akgl_program_fbxfer_draw(
    ps_video.program_fbxfer,ps_video.framebuffer,
    ps_video.dstx,ps_video.dsty,ps_video.dstw,ps_video.dsth
  )<0) return -1;

  #if PS_USE_macwm
    if (ps_macwm_flush_video()<0) return -1;
  #elif PS_USE_bcm
    if (ps_bcm_swap()<0) return -1;
  #elif PS_USE_glx
    if (ps_glx_swap()<0) return -1;
  #elif PS_USE_mswm
    if (ps_mswm_swap()<0) return -1;
  #endif
  
  return 0;
}

int ps_video_draw_to_framebuffer() {
  if (!ps_video.init) return -1;

  if (akgl_framebuffer_use(ps_video.framebuffer)<0) return -1;
  if (ps_video_draw_layers(1)<0) return -1;
  if (akgl_framebuffer_use(0)<0) return -1;
  
  return 0;
}

/* Redraw framebuffer with only the game layer.
 */

int ps_video_redraw_game_only() {
  if (!ps_video.init) return -1;
  if (akgl_framebuffer_use(ps_video.framebuffer)<0) return -1;
  if (ps_video_draw_layers(1)<0) return -1;
  if (akgl_framebuffer_use(0)<0) return -1;
  return 0;
}

/* Capture framebuffer to new texture.
 */
 
struct akgl_texture *ps_video_capture_framebuffer() {
  if (!ps_video.init) return 0;

  if (akgl_framebuffer_use(ps_video.framebuffer)<0) return 0;
 	uint8_t pixels[PS_SCREENW*PS_SCREENH*4];
 	glReadPixels(0,0,PS_SCREENW,PS_SCREENH,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if (akgl_framebuffer_use(0)<0) return 0;

 	struct akgl_texture *texture=akgl_texture_new();
 	if (!texture) return 0;
 	if (akgl_texture_load(texture,pixels,AKGL_FMT_RGBA8,PS_SCREENW,PS_SCREENH)<0) {
 	  akgl_texture_del(texture);
 	  return 0;
 	}
  
  return texture;
}

/* Flip image vertically.
 */

static void ps_video_flip_image(uint8_t *pixels,int stride,int h,uint8_t *buffer) {
  uint8_t *a=pixels;
  uint8_t *b=pixels+stride*h;
  int c=h>>1;
  while (c-->0) {
    b-=stride;
    memcpy(buffer,a,stride);
    memcpy(a,b,stride);
    memcpy(b,buffer,stride);
    a+=stride;
  }
}

/* Capture framebuffer to raw pixels.
 */
 
int ps_video_capture_framebuffer_raw(void *pixelspp,int game_only) {
  if (!ps_video.init) return -1;
  if (!pixelspp) return -1;

  if (game_only) {
    if (ps_video_redraw_game_only()<0) return -1;
  }

  int bytesize=PS_SCREENW*PS_SCREENH*4;
  void *pixels=malloc(bytesize);
  if (!pixels) return -1;

  if (akgl_framebuffer_use(ps_video.framebuffer)<0) {
    free(pixels);
    return -1;
  }
  glReadPixels(0,0,PS_SCREENW,PS_SCREENH,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if (akgl_framebuffer_use(0)<0) {
    free(pixels);
    return -1;
  }

  uint8_t buffer[PS_SCREENW*4];
  ps_video_flip_image(pixels,PS_SCREENW*4,PS_SCREENH,buffer);

  *(void**)pixelspp=pixels;
  return bytesize;
}

/* Recalculate destination rect for final framebuffer transfer.
 * We modify (dstx,dsty,dstw,dsth) based on (winw,winh).
 */

static int ps_video_recalculate_framebuffer_output_bounds() {
  if ((ps_video.winw<1)||(ps_video.winh<1)) return 0;

  /* Consider tolerance, can we use the whole window? */
  double targetaspect=(double)ps_video.winw/(double)ps_video.winh;
  double sourceaspect=(double)PS_SCREENW/(double)PS_SCREENH;
  double aspect_error;
  if (targetaspect>sourceaspect) aspect_error=sourceaspect/targetaspect;
  else aspect_error=targetaspect/sourceaspect;
  aspect_error=1.0-aspect_error;
  if (aspect_error<PS_VIDEO_ASPECT_TOLERANCE) {
    //ps_log(VIDEO,DEBUG,"Aspect within tolerance (%.3f~=%.3f, tolerance=%.3f).",sourceaspect,targetaspect,PS_VIDEO_ASPECT_TOLERANCE);
    ps_video.dstx=0;
    ps_video.dsty=0;
    ps_video.dstw=ps_video.winw;
    ps_video.dsth=ps_video.winh;
    return 0;
  }

  /* We'll use one window axis as-is.
   * For each, calculate the ideal size for the other.
   */
  int wforh=(PS_SCREENW*ps_video.winh)/PS_SCREENH;
  int hforw=(PS_SCREENH*ps_video.winw)/PS_SCREENW;

  /* Match the axis where the alternate is smaller or equal to the output. */
  if (wforh<=ps_video.winw) {
    ps_video.dstw=wforh;
    ps_video.dsth=ps_video.winh;
  } else {
    ps_video.dstw=ps_video.winw;
    ps_video.dsth=hforw;
  }

  /* Center output in the window. */
  ps_video.dstx=(ps_video.winw>>1)-(ps_video.dstw>>1);
  ps_video.dsty=(ps_video.winh>>1)-(ps_video.dsth>>1);

  return 0;
}

/* Respond to resize.
 */

int ps_video_resized(int w,int h) {
  if ((w<1)||(h<1)) return 0;
  ps_video.winw=w;
  ps_video.winh=h;
  if (akgl_set_screen_size(w,h)<0) return -1;
  if (ps_video_recalculate_framebuffer_output_bounds()<0) return -1;
  return 0;
}

/* Translate coordinates between framebuffer and window.
 */
 
int ps_video_point_framebuffer_from_window(int *fbx,int *fby,int winx,int winy) {
  if (fbx) {
    if (ps_video.dstw<1) return -1;
    *fbx=((winx-ps_video.dstx)*PS_SCREENW)/ps_video.dstw;
  }
  if (fby) {
    if (ps_video.dsth<1) return -1;
    *fby=((winy-ps_video.dsty)*PS_SCREENH)/ps_video.dsth;
  }
  return 0;
}

int ps_video_point_window_from_framebuffer(int *winx,int *winy,int fbx,int fby) {
  if (winx) {
    if (ps_video.dstw<1) return -1;
    *winx=ps_video.dstx+(fbx*ps_video.dstw)/PS_SCREENW;
  }
  if (winy) {
    if (ps_video.dsth<1) return -1;
    *winy=ps_video.dsty+(fby*ps_video.dsth)/PS_SCREENH;
  }
  return 0;
}

/* Layer list.
 */
 
int ps_video_install_layer(struct ps_video_layer *layer,int p) {
  if (!layer) return -1;
  if (!ps_video.init) return -1;
  int i=ps_video.layerc; while (i-->0) if (ps_video.layerv[i]==layer) return -1;
  if ((p<0)||(p>ps_video.layerc)) p=ps_video.layerc;

  if (ps_video.layerc>=ps_video.layera) {
    int na=ps_video.layera+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ps_video.layerv,sizeof(void*)*na);
    if (!nv) return -1;
    ps_video.layerv=nv;
    ps_video.layera=na;
  }

  if (ps_video_layer_ref(layer)<0) return -1;

  memmove(ps_video.layerv+p+1,ps_video.layerv+p,sizeof(void*)*(ps_video.layerc-p));
  ps_video.layerv[p]=layer;
  ps_video.layerc++;

  return 0;
}

int ps_video_uninstall_layer(struct ps_video_layer *layer) {
  if (!layer) return -1;
  int i=ps_video.layerc; for (;i-->0;) {
    if (ps_video.layerv[i]==layer) {
      ps_video.layerc--;
      memmove(ps_video.layerv+i,ps_video.layerv+i+1,sizeof(void*)*(ps_video.layerc-i));
      ps_video_layer_del(layer);
      return 0;
    }
  }
  return -1;
}

int ps_video_count_layers() {
  return ps_video.layerc;
}

struct ps_video_layer *ps_video_get_layer(int p) {
  if ((p<0)||(p>=ps_video.layerc)) return 0;
  return ps_video.layerv[p];
}

/* Vertex assembly.
 */
 
int ps_video_vtxv_reset(int size) {
  if (size<1) return -1;
  if (!ps_video.init) return -1;
  ps_video.vtxsize=size;
  ps_video.vtxc=0;
  return 0;
}

void *ps_video_vtxv_add(int addc) {
  if (addc<1) return 0;
  if (ps_video.vtxsize<1) return 0;
  if (ps_video.vtxc>INT_MAX-addc) return 0;
  int na=ps_video.vtxc+addc;
  if (na>INT_MAX/ps_video.vtxsize) return 0;
  na*=ps_video.vtxsize;
  if (na>ps_video.vtxa) {
    if (na<INT_MAX-1024) na=(na+1024)&~1023;
    void *nv=realloc(ps_video.vtxv,na);
    if (!nv) return 0;
    ps_video.vtxv=nv;
    ps_video.vtxa=na;
  }
  void *result=ps_video.vtxv+ps_video.vtxc*ps_video.vtxsize;
  ps_video.vtxc+=addc;
  return result;
}

/* Fullscreen toggle.
 */
 
int ps_video_supports_fullscreen_toggle() {
  #if PS_USE_glx
    return 1;
  #elif PS_USE_macwm
    return 1;
  #elif PS_USE_mswm
    return 1;
  #else
    return 0;
  #endif
}

int ps_video_toggle_fullscreen() {
  if (!ps_video.init) return -1;
  #if PS_USE_glx
    return ps_glx_set_fullscreen(-1);
  #elif PS_USE_macwm
    ps_log(VIDEO,WARN,"TODO: Toggle fullscreen for macwm");
  #elif PS_USE_mswm
    ps_log(VIDEO,WARN,"TODO: Toggle fullscreen for mswm");
  #else
    ps_log(VIDEO,WARN,"Requested to toggle fullscreen on platform that doesn't support it.");
  #endif
  return 0;
}
