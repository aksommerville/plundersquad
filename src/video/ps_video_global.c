#include "ps_video_internal.h"

struct ps_video ps_video={0};

/* Initialize akgl and the associated objects.
 */

static int ps_video_init_akgl() {
  if (akgl_init()<0) return -1;
  if (akgl_set_screen_size(ps_video.winw,ps_video.winh)<0) return -1;
  if (akgl_set_glsl_version(PS_GLSL_VERSION)<0) return -1;

  if (!(ps_video.framebuffer=akgl_framebuffer_new())) return -1;
  if (akgl_framebuffer_resize(ps_video.framebuffer,PS_SCREENW,PS_SCREENH)<0) return -1;

  if (!(ps_video.program_raw=akgl_program_raw_new())) return -1;
  if (!(ps_video.program_fbxfer=akgl_program_fbxfer_new())) return -1;
  if (!(ps_video.program_tex=akgl_program_tex_new())) return -1;
  if (!(ps_video.program_mintile=akgl_program_mintile_new())) return -1;
  if (!(ps_video.program_maxtile=akgl_program_maxtile_new())) return -1;
  if (!(ps_video.program_textile=akgl_program_textile_new())) return -1;

  if (!(ps_video.texture_minfont=akgl_texture_new())) return -1;
  if (ps_video_load_minfont(ps_video.texture_minfont)<0) return -1;

  return 0;
}

/* Init.
 */

int ps_video_init() {

  if (ps_video.init) return -1;
  memset(&ps_video,0,sizeof(struct ps_video));
  ps_video.init=1;

  ps_video.winw=PS_SCREENW<<1;
  ps_video.winh=PS_SCREENH<<1;
  ps_video.dstx=0;
  ps_video.dsty=0;
  ps_video.dstw=ps_video.winw;
  ps_video.dsth=ps_video.winh;

  #if PS_USE_macwm
    if (ps_macwm_init(ps_video.winw,ps_video.winh,0,"Plunder Squad")<0) {
      ps_video_quit();
      return -1;
    }
  #endif

  if (ps_video_init_akgl()<0) {
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

static int ps_video_draw_layers() {

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
  }

  return 0;
}

/* Update.
 */

int ps_video_update() {
  if (!ps_video.init) return -1;

  /* Draw scene into the framebuffer. */
  if (akgl_framebuffer_use(ps_video.framebuffer)<0) return -1;
  if (ps_video_draw_layers()<0) return -1;
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
  #endif
  
  return 0;
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
