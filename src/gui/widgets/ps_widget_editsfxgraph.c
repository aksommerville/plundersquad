/* ps_widget_editsfxgraph.c
 * Interactive line graph representing a single stream of step or trim changes.
 * Parent: editsfxchan.
 */

#include "ps.h"
#include "gui/ps_gui.h"
#include "resedit/ps_iwg.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "akau/akau.h"

static int ps_editsfxgraph_rebuild(struct ps_widget *widget);

/* Widget definition.
 */

struct ps_editsfxgraph_point {
  int x,y;
  int cmdp;
};

struct ps_widget_editsfxgraph {
  struct ps_widget hdr;
  int chanid; // Must be same as parent.
  int k; // AKAU_WAVEGEN_K_*
  struct ps_editsfxgraph_point *ptv;
  int ptc,pta;
  void *vtxv;
  int vtxa; // vertex buffer size in bytes
  int mousex,mousey;
  int trackpt; // <0 or index in (ptv) while changing
  int scale_time;
  double scale_va,scale_vz;
};

#define WIDGET ((struct ps_widget_editsfxgraph*)widget)

/* Get wavegen model from parent.
 */

static struct ps_iwg *ps_editsfxgraph_get_iwg(const struct ps_widget *widget) {
  if (!widget) return 0;
  if (!(widget=widget->parent)) return 0;
  return ps_widget_editsfx_get_iwg(widget->parent);
}

/* Delete.
 */

static void _ps_editsfxgraph_del(struct ps_widget *widget) {
  if (WIDGET->ptv) free(WIDGET->ptv);
  if (WIDGET->vtxv) free(WIDGET->vtxv);
}

/* Initialize.
 */

static int _ps_editsfxgraph_init(struct ps_widget *widget) {
  widget->track_mouse=1;
  WIDGET->trackpt=-1;
  return 0;
}

/* Calculate maximum vertex buffer size and reallocate if necessary.
 */

static int ps_editsfxgraph_require_vtxv(struct ps_widget *widget) {
  int vtxsize=sizeof(struct akgl_vtx_mintile);
  if (sizeof(struct akgl_vtx_raw)>vtxsize) vtxsize=sizeof(struct akgl_vtx_raw); // It's not, but let's be clear.
  int reqa=vtxsize*WIDGET->ptc;
  if (reqa<=WIDGET->vtxa) return 0;
  void *nv=malloc(reqa);
  if (!nv) return -1;
  if (WIDGET->vtxv) free(WIDGET->vtxv);
  WIDGET->vtxv=nv;
  WIDGET->vtxa=reqa;
  return 0;
}

/* Draw line.
 */

static int ps_editsfxgraph_draw_line(struct ps_widget *widget,int x0,int y0) {
  x0+=widget->x;
  y0+=widget->y;
  uint8_t r=widget->fgrgba>>24;
  uint8_t g=widget->fgrgba>>16;
  uint8_t b=widget->fgrgba>>8;
  uint8_t a=widget->fgrgba;
  
  struct akgl_vtx_raw *vtxv=WIDGET->vtxv;
  int i=0;
  const struct ps_editsfxgraph_point *pt=WIDGET->ptv;
  for (;i<WIDGET->ptc;i++,pt++) {
    vtxv[i].x=x0+pt->x;
    vtxv[i].y=y0+pt->y;
    vtxv[i].r=r;
    vtxv[i].g=g;
    vtxv[i].b=b;
    vtxv[i].a=a;
  }

  if (ps_video_draw_line_strip(vtxv,WIDGET->ptc)<0) return -1;
  
  return 0;
}

/* Draw points.
 */

static int ps_editsfxgraph_draw_points(struct ps_widget *widget,int x0,int y0) {
  x0+=widget->x;
  y0+=widget->y;
  
  struct akgl_vtx_mintile *vtxv=WIDGET->vtxv;
  int i=WIDGET->ptc,vtxc=0;
  const struct ps_editsfxgraph_point *pt=WIDGET->ptv;
  for (;i-->0;pt++) {
    if (pt->cmdp<0) continue; // Don't draw points on implicit ones.
    vtxv[vtxc].x=x0+pt->x;
    vtxv[vtxc].y=y0+pt->y;
    vtxv[vtxc].tileid=0x05;
    vtxc++;
  }

  if (ps_video_draw_mintile(vtxv,vtxc,1)<0) return -1;
  
  return 0;
}

/* Draw.
 */

static int _ps_editsfxgraph_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (WIDGET->ptc) {
    if (ps_editsfxgraph_require_vtxv(widget)<0) return -1;
    if (ps_editsfxgraph_draw_line(widget,x0,y0)<0) return -1;
    if (ps_editsfxgraph_draw_points(widget,x0,y0)<0) return -1;
  }
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_editsfxgraph_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_editsfxgraph_pack(struct ps_widget *widget) {
  if (ps_editsfxgraph_rebuild(widget)<0) return -1;
  return 0;
}

/* Find point.
 */

static int ps_editsfxgraph_find_point(const struct ps_widget *widget,int x,int y) {
  int radius=3;
  const struct ps_editsfxgraph_point *pt=WIDGET->ptv;
  int i=0; for (;i<WIDGET->ptc;i++,pt++) {
    if (x<pt->x-radius) continue;
    if (x>pt->x+radius) continue;
    if (y<pt->y-radius) continue;
    if (y>pt->y+radius) continue;
    return i;
  }
  return -1;
}

/* Add point.
 */

static int ps_editsfxgraph_create_point(struct ps_widget *widget,int x,int y) {
  int time=(x*WIDGET->scale_time)/widget->w;
  double value=WIDGET->scale_va+((widget->h-y)*(WIDGET->scale_vz-WIDGET->scale_va))/widget->h;
  struct ps_iwg *iwg=ps_editsfxgraph_get_iwg(widget);
  if (!iwg) return -1;
  iwg->dirty=1;
  if (ps_iwg_add_command(iwg,time,WIDGET->chanid,WIDGET->k,value)<0) return -1;
  if (!widget->parent) return -1;
  if (ps_widget_pack(widget->parent->parent)<0) return -1; // Force UI to rebuild (all graphs)
  return 0;
}

/* Remove point.
 */

static int ps_editsfxgraph_remove_point(struct ps_widget *widget,int ptp) {
  struct ps_iwg *iwg=ps_editsfxgraph_get_iwg(widget);
  if (!iwg) return -1;
  iwg->dirty=1;
  if (ps_iwg_remove_commands(iwg,WIDGET->ptv[ptp].cmdp,1)<0) return -1;
  if (!widget->parent) return -1;
  if (ps_widget_pack(widget->parent->parent)<0) return -1; // Force UI to rebuild (all graphs)
  return 0;
}

/* Mouse events.
 */

static int _ps_editsfxgraph_mouseenter(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfxgraph_mouseexit(struct ps_widget *widget) {
  return 0;
}

static int _ps_editsfxgraph_mousedown(struct ps_widget *widget,int btnid) {
  if (btnid==1) {
    int ptp=ps_editsfxgraph_find_point(widget,WIDGET->mousex,WIDGET->mousey);
    if (ptp>=0) {
      WIDGET->trackpt=ptp;
    } else {
      if (ps_editsfxgraph_create_point(widget,WIDGET->mousex,WIDGET->mousey)<0) return -1;
    }
  } else if (btnid==3) {
    int ptp=ps_editsfxgraph_find_point(widget,WIDGET->mousex,WIDGET->mousey);
    if (ptp>=0) {
      if (ps_editsfxgraph_remove_point(widget,ptp)<0) return -1;
    }
  }
  return 0;
}

static int _ps_editsfxgraph_mouseup(struct ps_widget *widget,int btnid,int inbounds) {
  if (btnid==1) {
    if ((WIDGET->trackpt>=0)&&(WIDGET->trackpt<WIDGET->ptc)) {
      struct ps_iwg *iwg=ps_editsfxgraph_get_iwg(widget);
      if (iwg) {
        struct ps_editsfxgraph_point *pt=WIDGET->ptv+WIDGET->trackpt;
        if ((pt->cmdp>=0)&&(pt->cmdp<iwg->cmdc)) {
          int ntime=(pt->x*WIDGET->scale_time)/widget->w;
          iwg->cmdv[pt->cmdp].v=WIDGET->scale_va+((widget->h-pt->y)*(WIDGET->scale_vz-WIDGET->scale_va))/widget->h;
          iwg->dirty=1;
          if (ps_iwg_adjust_command_time(iwg,pt->cmdp,ntime)<0) return -1;
          if (!widget->parent) return -1;
          if (ps_widget_pack(widget->parent->parent)<0) return -1; // Force UI to rebuild (all graphs)
        }
      }
      WIDGET->trackpt=-1;
    }
  }
  return 0;
}

static int _ps_editsfxgraph_mousewheel(struct ps_widget *widget,int dx,int dy) {
  return 0;
}

static int _ps_editsfxgraph_mousemove(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;
  if (WIDGET->trackpt>=0) {
    struct ps_editsfxgraph_point *pt=WIDGET->ptv+WIDGET->trackpt;
    pt->x=x;
    pt->y=y;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_editsfxgraph={
  .name="editsfxgraph",
  .objlen=sizeof(struct ps_widget_editsfxgraph),
  .del=_ps_editsfxgraph_del,
  .init=_ps_editsfxgraph_init,
  .draw=_ps_editsfxgraph_draw,
  .measure=_ps_editsfxgraph_measure,
  .pack=_ps_editsfxgraph_pack,
  .mouseenter=_ps_editsfxgraph_mouseenter,
  .mouseexit=_ps_editsfxgraph_mouseexit,
  .mousedown=_ps_editsfxgraph_mousedown,
  .mouseup=_ps_editsfxgraph_mouseup,
  .mousewheel=_ps_editsfxgraph_mousewheel,
  .mousemove=_ps_editsfxgraph_mousemove,
};

/* Add point.
 */

static int ps_editsfxgraph_add_point(struct ps_widget *widget,int x,int y,int cmdp) {

  if (WIDGET->ptc>=WIDGET->pta) {
    int na=WIDGET->pta+8;
    if (na>INT_MAX/sizeof(struct ps_editsfxgraph_point)) return -1;
    void *nv=realloc(WIDGET->ptv,sizeof(struct ps_editsfxgraph_point)*na);
    if (!nv) return -1;
    WIDGET->ptv=nv;
    WIDGET->pta=na;
  }

  struct ps_editsfxgraph_point *pt=WIDGET->ptv+WIDGET->ptc++;
  pt->x=x;
  pt->y=y;
  pt->cmdp=cmdp;

  return 0;
}

/* Rebuild visual model from wavegen.
 */

static int ps_editsfxgraph_rebuild(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_editsfxgraph)) return -1;
  WIDGET->ptc=0;
  struct ps_iwg *iwg=ps_editsfxgraph_get_iwg(widget);
  if (!iwg) return 0;
  if ((WIDGET->chanid<0)||(WIDGET->chanid>=iwg->chanc)) return 0;

  /* Gather stats. */
  int fldc=0;
  double vmin,vmax,vlast;
  int tmin,tmax;
  const struct ps_iwg_command *cmd=iwg->cmdv;
  int cmdp=0; for (;cmdp<iwg->cmdc;cmdp++,cmd++) {
    if (cmd->chanid!=WIDGET->chanid) continue;
    if (cmd->k!=WIDGET->k) continue;
    if (!fldc) {
      vmin=cmd->v;
      vmax=cmd->v;
      tmin=cmd->time;
      tmax=cmd->time;
    } else {
      if (cmd->v<vmin) vmin=cmd->v;
      else if (cmd->v>vmax) vmax=cmd->v;
      tmax=cmd->time; // No need to check; they are sorted by time.
    }
    fldc++;
    vlast=cmd->v;
  }
  if (fldc<1) return 0;

  /* Establish logical values for my edges. */
  int xatime=0;
  int xztime=iwg->cmdv[iwg->cmdc-1].time; // All graphs should be on the same time scale.
  double yav,yzv;
  if (WIDGET->k==AKAU_WAVEGEN_K_TRIM) { // Trim are always in 0..1, don't scale based on input range.
    yav=0.0;
    yzv=1.0;
  } else { // Otherwise (ie step), show the whole range plus 10% on each side.
    double range=vmax-vmin;
    if (range<1.0) range=1.0; // Entirely possible that only one step value is present, don't panic.
    yav=vmin-range/10.0;
    yzv=vmax+range/10.0;
  }
  WIDGET->scale_time=xztime;
  WIDGET->scale_va=yav;
  WIDGET->scale_vz=yzv;

  /* Define a point for each entry and two implicit end points. */
  if (ps_editsfxgraph_add_point(widget,0,widget->h,-1)<0) return -1;
  for (cmdp=0,cmd=iwg->cmdv;cmdp<iwg->cmdc;cmdp++,cmd++) {
    if (cmd->chanid!=WIDGET->chanid) continue;
    if (cmd->k!=WIDGET->k) continue;
    int x=(cmd->time*widget->w)/xztime;
    int y=widget->h-((cmd->v-yav)*widget->h)/(yzv-yav);
    if (ps_editsfxgraph_add_point(widget,x,y,cmdp)<0) return -1;
  }
  int y=widget->h-((vlast-yav)*widget->h)/(yzv-yav);
  if (ps_editsfxgraph_add_point(widget,widget->w,y,-1)<0) return -1;

  return 0;
}

/* Set field.
 */
 
int ps_widget_editsfxgraph_set_field(struct ps_widget *widget,int chanid,int k) {
  if (!widget||(widget->type!=&ps_widget_type_editsfxgraph)) return -1;
  WIDGET->chanid=chanid;
  WIDGET->k=k;
  if (ps_editsfxgraph_rebuild(widget)<0) return -1;
  return 0;
}
