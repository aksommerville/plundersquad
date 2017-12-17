#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "akau/akau.h"
#include <math.h>

// Sanity limits on display size. Below this, we don't render anything.
#define PS_WAVEVIEW_MINW    20
#define PS_WAVEVIEW_MINH    20

/* Object definition.
 */

struct ps_widget_waveview {
  struct ps_widget hdr;
  struct akgl_vtx_raw *vtxv;
  int vtxc,vtxa;
};

#define WIDGET ((struct ps_widget_waveview*)widget)

/* Delete.
 */

static void _ps_waveview_del(struct ps_widget *widget) {
  if (WIDGET->vtxv) free(WIDGET->vtxv);
}

/* Initialize.
 */

static int _ps_waveview_init(struct ps_widget *widget) {
  widget->bgrgba=0x002000ff;
  widget->fgrgba=0xffff00ff;
  return 0;
}

/* Grow vertex buffer.
 */

static int ps_waveview_vtxv_require(struct ps_widget *widget,int addc) {
  if (addc<1) return 0;
  if (WIDGET->vtxc>INT_MAX-addc) return -1;
  int na=WIDGET->vtxc+addc;
  if (na<=WIDGET->vtxa) return 0;
  if (na<INT_MAX-32) na=(na+32)&~31;
  if (na>INT_MAX/sizeof(struct akgl_vtx_raw)) return -1;
  void *nv=realloc(WIDGET->vtxv,sizeof(struct akgl_vtx_raw)*na);
  if (!nv) return -1;
  WIDGET->vtxv=nv;
  WIDGET->vtxa=na;
  return 0;
}

/* Draw.
 */

static void ps_waveview_offset_vertices(struct ps_widget *widget,int dx,int dy) {
  struct akgl_vtx_raw *vtx=WIDGET->vtxv;
  int i=WIDGET->vtxc; for (;i-->0;vtx++) {
    vtx->x+=dx;
    vtx->y+=dy;
  }
}

static int _ps_waveview_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;

  if (WIDGET->vtxc>0) {
    parentx+=widget->x;
    parenty+=widget->y;
    ps_waveview_offset_vertices(widget,parentx,parenty);
    if (ps_video_draw_line_strip(WIDGET->vtxv,WIDGET->vtxc)<0) return -1;
    ps_waveview_offset_vertices(widget,-parentx,-parenty);
  }
  
  return 0;
}

/* Rebuild vertices for presets.
 */

static int ps_waveview_rebuild_sine(struct ps_widget *widget) {
  if (ps_waveview_vtxv_require(widget,widget->w)<0) return -1;
  int midy=widget->h>>1;
  int x=0; for (;x<widget->w;x++) {
    double p=(x*M_PI*2.0)/widget->w;
    double s=sin(p);
    int y=midy-s*midy;
    struct akgl_vtx_raw *vtx=WIDGET->vtxv+WIDGET->vtxc++;
    vtx->x=x;
    vtx->y=y;
  }
  return 0;
}

static int ps_waveview_rebuild_square(struct ps_widget *widget) {
  if (ps_waveview_vtxv_require(widget,6)<0) return -1;
  int midx=widget->w>>1;
  int midy=widget->h>>1;
  WIDGET->vtxv[0].x=1;
  WIDGET->vtxv[0].y=midy;
  WIDGET->vtxv[1].x=1;
  WIDGET->vtxv[1].y=0;
  WIDGET->vtxv[2].x=midx;
  WIDGET->vtxv[2].y=0;
  WIDGET->vtxv[3].x=midx;
  WIDGET->vtxv[3].y=widget->h-1;
  WIDGET->vtxv[4].x=widget->w-1;
  WIDGET->vtxv[4].y=widget->h-1;
  WIDGET->vtxv[5].x=widget->w-1;
  WIDGET->vtxv[5].y=midy;
  WIDGET->vtxc=6;
  return 0;
}

static int ps_waveview_rebuild_saw(struct ps_widget *widget) {
  if (ps_waveview_vtxv_require(widget,2)<0) return -1;
  WIDGET->vtxv[0].x=0;
  WIDGET->vtxv[0].y=0;
  WIDGET->vtxv[1].x=widget->w-1;
  WIDGET->vtxv[1].y=widget->h-1;
  WIDGET->vtxc=2;
  return 0;
}

/* Rebuild vertices for harmonics.
 */

static int ps_waveview_rebuild_harmonics(struct ps_widget *widget) {

  const struct akau_fpcm *fpcm=ps_widget_wave_get_fpcm(widget);
  if (!fpcm) return -1;
  int samplec=akau_fpcm_get_sample_count(fpcm);
  if (samplec<1) return -1;
  
  if (ps_waveview_vtxv_require(widget,widget->w)<0) return -1;
  int midy=widget->h>>1;
  int x=0; for (;x<widget->w;x++) {
    int p=(x*samplec)/widget->w;
    if (p<0) p=0; else if (p>=samplec) p=samplec-1;
    double s=akau_fpcm_get_sample(fpcm,p);
    if ((s<-1.0)||(s>1.0)) ps_log(EDIT,INFO,"%d/%d = %f",p,samplec,s);//XXX
    int y=midy-s*midy;
    struct akgl_vtx_raw *vtx=WIDGET->vtxv+WIDGET->vtxc++;
    vtx->x=x;
    vtx->y=y;
  }
  return 0;
}

/* Pack.
 */

static int _ps_waveview_pack(struct ps_widget *widget) {

  if (widget->w<PS_WAVEVIEW_MINW) return 0;
  if (widget->h<PS_WAVEVIEW_MINH) return 0;

  const struct ps_wave_ui_model *model=ps_widget_wave_get_model(widget);
  if (!model) return -1;
  WIDGET->vtxc=0;
  
  switch (model->preset) {
    case PS_WIDGET_WAVE_PRESET_SINE: if (ps_waveview_rebuild_sine(widget)<0) return -1; break;
    case PS_WIDGET_WAVE_PRESET_SQUARE: if (ps_waveview_rebuild_square(widget)<0) return -1; break;
    case PS_WIDGET_WAVE_PRESET_SAW: if (ps_waveview_rebuild_saw(widget)<0) return -1; break;
    case PS_WIDGET_WAVE_PRESET_NOISE: break; // No sense drawing anything for noise.
    case PS_WIDGET_WAVE_PRESET_HARMONICS: if (ps_waveview_rebuild_harmonics(widget)<0) return -1; break;
  }

  uint8_t r=widget->fgrgba>>24;
  uint8_t g=widget->fgrgba>>16;
  uint8_t b=widget->fgrgba>>8;
  uint8_t a=widget->fgrgba;
  struct akgl_vtx_raw *vtx=WIDGET->vtxv;
  int i=WIDGET->vtxc; for (;i-->0;vtx++) {
    vtx->r=r;
    vtx->g=g;
    vtx->b=b;
    vtx->a=a;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_waveview={

  .name="waveview",
  .objlen=sizeof(struct ps_widget_waveview),

  .del=_ps_waveview_del,
  .init=_ps_waveview_init,

  .draw=_ps_waveview_draw,
  .pack=_ps_waveview_pack,

};

/* Update from parent's model.
 */

int ps_widget_waveview_model_changed(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_waveview)) return -1;
  if (_ps_waveview_pack(widget)<0) return -1;
  return 0;
}
