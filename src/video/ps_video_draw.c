#include "ps_video_internal.h"
#include "scenario/ps_grid.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"

/* Draw text.
 */
 
int ps_video_text_begin() {
  if (ps_video_vtxv_reset(sizeof(struct akgl_vtx_textile))<0) return -1;
  return 0;
}

int ps_video_text_add(int size,uint32_t rgba,int x,int y,const char *src,int srcc) {
  if (size<1) return 0;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  if (!(rgba&0xff)) return 0;
  if (ps_video.vtxsize!=sizeof(struct akgl_vtx_textile)) return -1;

  struct akgl_vtx_textile *vtxv=ps_video_vtxv_add(srcc);
  if (!vtxv) return -1;

  vtxv[0].x=x;
  vtxv[0].y=y;
  vtxv[0].tileid=src[0];
  vtxv[0].r=rgba>>24;
  vtxv[0].g=rgba>>16;
  vtxv[0].b=rgba>>8;
  vtxv[0].a=rgba;
  vtxv[0].size=size;
  struct akgl_vtx_textile *vtx0=vtxv;

  src++;
  vtxv++;
  srcc--;

  int dx=size>>1;
  for (;srcc-->0;src++,vtxv++) {
    x+=dx;
    memcpy(vtxv,vtx0,sizeof(struct akgl_vtx_textile));
    vtxv->x=x;
    vtxv->tileid=*src;
  }

  return 0;
}

int ps_video_text_addf(int size,uint32_t rgba,int x,int y,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return ps_video_text_addfv(size,rgba,x,y,fmt,vargs);
}

int ps_video_text_addfv(int size,uint32_t rgba,int x,int y,const char *fmt,va_list vargs) {
  char buf[256];
  int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
  if ((bufc<0)||(bufc>=sizeof(buf))) return -1;
  return ps_video_text_add(size,rgba,x,y,buf,bufc);
}

int ps_video_text_end() {
  if (ps_video.vtxsize!=sizeof(struct akgl_vtx_textile)) return -1;
  if (akgl_program_textile_draw(
    ps_video.program_textile,ps_video.texture_minfont,
    (struct akgl_vtx_textile*)ps_video.vtxv,ps_video.vtxc
  )<0) return -1;
  return 0;
}

/* Draw grid.
 */
 
int ps_video_draw_grid(const struct ps_grid *grid) {
  if (!grid) return -1;
  if (ps_video_vtxv_reset(sizeof(struct akgl_vtx_mintile))<0) return -1;
  struct akgl_vtx_mintile *vtxv=ps_video_vtxv_add(PS_GRID_SIZE);
  if (!vtxv) return -1;

  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,grid->cellv[0].tsid);
  if (!tilesheet) {
    ps_log(VIDEO,ERROR,"Tilesheet %d not found, required for grid.",grid->cellv[0].tsid);
    return -1;
  }

  int x=PS_TILESIZE>>1,y=PS_TILESIZE>>1;
  struct akgl_vtx_mintile *vtx=vtxv;
  const struct ps_grid_cell *cell=grid->cellv;
  int i=PS_GRID_SIZE; for (;i-->0;vtx++,cell++) {
    vtx->tileid=cell->tileid;
    vtx->x=x;
    vtx->y=y;
    x+=PS_TILESIZE;
    if (x>PS_SCREENW) {
      x=PS_TILESIZE>>1;
      y+=PS_TILESIZE;
    }
  }

  if (akgl_program_mintile_draw(
    ps_video.program_mintile,tilesheet->texture,vtxv,PS_GRID_SIZE,PS_TILESIZE
  )<0) return -1;
  
  return 0;
}

/* Draw sprite group.
 */

static int ps_video_commit_sprites(int tsid) {
  if (ps_video.vtxc<1) return 0;

  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,tsid);
  if (!tilesheet) {
    ps_log(RES,ERROR,"Tilesheet %d not found.",tsid);
    return -1;
  }

  if (akgl_program_maxtile_draw(
    ps_video.program_maxtile,tilesheet->texture,(struct akgl_vtx_maxtile*)ps_video.vtxv,ps_video.vtxc
  )<0) return -1;
  
  ps_video.vtxc=0;
  return 0;
}
 
int ps_video_draw_sprites(const struct ps_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<1) return 0;
  if (ps_video_vtxv_reset(sizeof(struct akgl_vtx_maxtile))<0) return -1;

  int i=0,tsid=-1;
  for (;i<grp->sprc;i++) {
    struct ps_sprite *spr=grp->sprv[i];
    if (spr->tsid!=tsid) {
      if (ps_video_commit_sprites(tsid)<0) return -1;
      tsid=spr->tsid;
    }

   _again_:;
    int vtxa=(ps_video.vtxa/sizeof(struct akgl_vtx_maxtile))-ps_video.vtxc;
    if (vtxa<1) {
      int na=ps_video.vtxa+sizeof(struct akgl_vtx_maxtile)*32;
      void *nv=realloc(ps_video.vtxv,sizeof(struct akgl_vtx_maxtile)*na);
      if (!nv) return -1;
      ps_video.vtxv=nv;
      ps_video.vtxa=na;
      vtxa=32;
    }
    struct akgl_vtx_maxtile *vtxv=((struct akgl_vtx_maxtile*)ps_video.vtxv)+ps_video.vtxc;

    int vtxc=ps_sprite_draw(vtxv,vtxa,spr);
    if (vtxc<0) return -1;

    if (vtxc>vtxa) {
      int na=ps_video.vtxa+sizeof(struct akgl_vtx_maxtile)*vtxc;
      void *nv=realloc(ps_video.vtxv,sizeof(struct akgl_vtx_maxtile)*na);
      if (!nv) return -1;
      ps_video.vtxv=nv;
      ps_video.vtxa=na;
      goto _again_;
    }
    ps_video.vtxc+=vtxc;
  }

  if (ps_video_commit_sprites(tsid)<0) return -1;
  
  return 0;
}
