#include "ps_video_internal.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_region.h"
#include "game/ps_sprite.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "sdraw/ps_sdraw.h"

/* Draw rectangle.
 */
 
int ps_video_draw_rect(int x,int y,int w,int h,uint32_t rgba) {

  struct ps_sdraw_image *dst=akgl_get_output_image();
  if (dst) {
    if (ps_sdraw_draw_rect(dst,x,y,w,h,ps_sdraw_rgba32(rgba))<0) return -1;
    return 0;
  }
  
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  if (!a) return 0;

  if (ps_video_vtxv_triangle_require(6)<0) return -1;
  struct akgl_vtx_raw *vtxv=ps_video.vtxv_triangle+ps_video.vtxc_triangle;
  ps_video.vtxc_triangle+=6;

  int i; for (i=0;i<6;i++) {
    vtxv[i].r=r;
    vtxv[i].g=g;
    vtxv[i].b=b;
    vtxv[i].a=a;
  }
  vtxv[0].x=x;
  vtxv[0].y=y;
  vtxv[1].x=x;
  vtxv[1].y=y+h;
  vtxv[2].x=x+w;
  vtxv[2].y=y;
  vtxv[3].x=x+w;
  vtxv[3].y=y;
  vtxv[4].x=x;
  vtxv[4].y=y+h;
  vtxv[5].x=x+w;
  vtxv[5].y=y+h;
  
  return 0;
}

int ps_video_draw_horz_gradient(int x,int y,int w,int h,uint32_t rgba_left,uint32_t rgba_right) {

  struct ps_sdraw_image *dst=akgl_get_output_image();
  if (dst) {
    if (ps_sdraw_draw_horz_gradient(dst,x,y,w,h,ps_sdraw_rgba32(rgba_left),ps_sdraw_rgba32(rgba_right))<0) return -1;
    return 0;
  }
  
  uint8_t r1=rgba_left>>24,g1=rgba_left>>16,b1=rgba_left>>8,a1=rgba_left;
  uint8_t r2=rgba_right>>24,g2=rgba_right>>16,b2=rgba_right>>8,a2=rgba_right;
  struct akgl_vtx_raw vtxv[4]={
    {x  ,y  ,r1,g1,b1,a1},
    {x+w,y  ,r2,g2,b2,a2},
    {x  ,y+h,r1,g1,b1,a1},
    {x+w,y+h,r2,g2,b2,a2},
  };
  return akgl_program_raw_draw_triangle_strip(ps_video.program_raw,vtxv,4);
}

int ps_video_draw_vert_gradient(int x,int y,int w,int h,uint32_t rgba_top,uint32_t rgba_bottom) {

  struct ps_sdraw_image *dst=akgl_get_output_image();
  if (dst) {
    if (ps_sdraw_draw_vert_gradient(dst,x,y,w,h,ps_sdraw_rgba32(rgba_top),ps_sdraw_rgba32(rgba_bottom))<0) return -1;
    return 0;
  }
  
  uint8_t r1=rgba_top>>24,g1=rgba_top>>16,b1=rgba_top>>8,a1=rgba_top;
  uint8_t r2=rgba_bottom>>24,g2=rgba_bottom>>16,b2=rgba_bottom>>8,a2=rgba_bottom;
  struct akgl_vtx_raw vtxv[4]={
    {x  ,y  ,r1,g1,b1,a1},
    {x+w,y  ,r1,g1,b1,a1},
    {x  ,y+h,r2,g2,b2,a2},
    {x+w,y+h,r2,g2,b2,a2},
  };
  return akgl_program_raw_draw_triangle_strip(ps_video.program_raw,vtxv,4);
}

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

  if (ps_video_vtxv_textile_require(srcc)<0) return -1;
  struct akgl_vtx_textile *vtxv=ps_video.vtxv_textile+ps_video.vtxc_textile;

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
  ps_video.vtxc_textile++;

  int dx=size>>1;
  for (;srcc-->0;src++,vtxv++) {
    x+=dx;
    memcpy(vtxv,vtx0,sizeof(struct akgl_vtx_textile));
    vtxv->x=x;
    vtxv->tileid=*src;
    ps_video.vtxc_textile++;
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

int ps_video_text_end(int resid) {
  if (ps_video.vtxsize!=sizeof(struct akgl_vtx_textile)) return -1;
  if (ps_video.vtxc<1) return 0;

  struct akgl_texture *font=0;
  if (resid>0) {
    struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,resid);
    if (tilesheet) font=tilesheet->texture;
  }
  if (!font) font=ps_video.texture_minfont;
  
  if (akgl_program_textile_draw(
    ps_video.program_textile,font,(struct akgl_vtx_textile*)ps_video.vtxv,ps_video.vtxc
  )<0) return -1;
  return 0;
}

/* Draw grid.
 */
 
int ps_video_draw_grid(const struct ps_grid *grid,int offx,int offy) {
  if (!grid) return -1;
  if (ps_video_vtxv_reset(sizeof(struct akgl_vtx_mintile))<0) return -1;
  struct akgl_vtx_mintile *vtxv=ps_video_vtxv_add(PS_GRID_SIZE);
  if (!vtxv) return -1;

  if (!grid->region) {
    ps_log(VIDEO,ERROR,"Can't draw grid without region (is null)");
    return -1;
  }
  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,grid->region->tsid);
  if (!tilesheet) {
    ps_log(VIDEO,ERROR,"Tilesheet %d not found, required for grid.",grid->region->tsid);
    return -1;
  }

  int x=PS_TILESIZE>>1,y=PS_TILESIZE>>1;
  x+=offx;
  y+=offy;
  struct akgl_vtx_mintile *vtx=vtxv;
  const struct ps_grid_cell *cell=grid->cellv;
  int i=PS_GRID_SIZE; for (;i-->0;vtx++,cell++) {
    vtx->tileid=cell->tileid;
    vtx->x=x;
    vtx->y=y;
    x+=PS_TILESIZE;
    if (x>PS_SCREENW+offx) {
      x=(PS_TILESIZE>>1)+offx;
      y+=PS_TILESIZE;
    }
  }

  if (akgl_program_mintile_draw(
    ps_video.program_mintile,tilesheet->texture,vtxv,PS_GRID_SIZE,PS_TILESIZE
  )<0) return -1;
  
  return 0;
}

/* Draw sprites.
 */
 
int ps_video_draw_sprites(const struct ps_sprgrp *grp,int offx,int offy) {
  if (!grp||(grp->sprc<1)) return 0;
  int i=0; for (i=0;i<grp->sprc;i++) {
    struct ps_sprite *spr=grp->sprv[i];

    /* Flush all commands if the tilesheet has changed. */
    if (ps_video.vtxc_maxtile&&(ps_video.tsid_maxtile!=spr->tsid)) {
      if (ps_video_flush_cached_drawing()<0) return -1;
    }
    ps_video.tsid_maxtile=spr->tsid;

    /* Let the sprite add its own vertices. */
    if (ps_video_vtxv_maxtile_require(1)<0) return -1;
    while (1) {
      int err=ps_sprite_draw(ps_video.vtxv_maxtile+ps_video.vtxc_maxtile,ps_video.vtxa_maxtile-ps_video.vtxc_maxtile,spr);
      if (err<0) return -1;
      if (ps_video.vtxc_maxtile>ps_video.vtxa_maxtile-err) {
        if (ps_video_vtxv_maxtile_require(err)<0) return -1;
      } else {
        int j=err; while (j-->0) {
          ps_video.vtxv_maxtile[ps_video.vtxc_maxtile+j].x+=offx;
          ps_video.vtxv_maxtile[ps_video.vtxc_maxtile+j].y+=offy;
        }
        ps_video.vtxc_maxtile+=err;
        break;
      }
    }
    
  }
  return 0;
}

/* Draw mintiles.
 */
 
int ps_video_draw_mintile(const struct akgl_vtx_mintile *vtxv,int vtxc,uint8_t tsid) {
  if ((vtxc<0)||(vtxc&&!vtxv)) return -1;

  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,tsid);
  if (!tilesheet) {
    ps_log(VIDEO,ERROR,"Tilesheet %d not found.",tsid);
    return -1;
  }

  if (akgl_program_mintile_draw(
    ps_video.program_mintile,tilesheet->texture,vtxv,vtxc,PS_TILESIZE
  )<0) return -1;
  
  return 0;
}

/* Draw maxtiles.
 */
 
int ps_video_draw_maxtile(const struct akgl_vtx_maxtile *vtxv,int vtxc,uint8_t tsid) {
  if ((vtxc<0)||(vtxc&&!vtxv)) return -1;

  struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,tsid);
  if (!tilesheet) {
    ps_log(VIDEO,ERROR,"Tilesheet %d not found.",tsid);
    return -1;
  }

  if (akgl_program_maxtile_draw(
    ps_video.program_maxtile,tilesheet->texture,vtxv,vtxc
  )<0) return -1;
  
  return 0;
}

/* Draw line strip.
 */
 
int ps_video_draw_line_strip(const struct akgl_vtx_raw *vtxv,int vtxc) {
  if ((vtxc<0)||(vtxc&&!vtxv)) return -1;

  if (akgl_program_raw_draw_line_strip(
    ps_video.program_raw,vtxv,vtxc,1
  )<0) return -1;

  return 0;
}

/* Draw entire texture into framebuffer.
 */
 
int ps_video_draw_texture(struct akgl_texture *texture,int x,int y,int w,int h) {

  struct ps_sdraw_image *dst=akgl_get_output_image();
  if (dst) {
    struct ps_sdraw_image *src=(struct ps_sdraw_image*)texture;
    if (w<0) { x+=w; w=-w; }
    if (h<0) { y+=h; h=-h; }
    if (ps_sdraw_blit(dst,x,y,w,h,src,0,0,src->w,src->h)<0) return -1;
    return 0;
  }

  struct akgl_vtx_tex vtxv[4]={
    {x  ,y  ,0,0},
    {x  ,y+h,0,1},
    {x+w,y  ,1,0},
    {x+w,y+h,1,1},
  };
  return akgl_program_tex_draw(ps_video.program_tex,texture,vtxv,4);
}

/* Flush all vertex caches to AKGL.
 */
 
int ps_video_flush_cached_drawing() {

  if (ps_video.vtxc_triangle) {
    if (akgl_program_raw_draw_triangles(ps_video.program_raw,ps_video.vtxv_triangle,ps_video.vtxc_triangle)<0) return -1;
    ps_video.vtxc_triangle=0;
  }

  if (ps_video.vtxc_maxtile) {
    struct ps_res_TILESHEET *tilesheet=ps_res_get(PS_RESTYPE_TILESHEET,ps_video.tsid_maxtile);
    if (!tilesheet) {
      ps_log(RES,ERROR,"Tilesheet %d not found.",ps_video.tsid_maxtile);
      return -1;
    }
    if (akgl_program_maxtile_draw(ps_video.program_maxtile,tilesheet->texture,ps_video.vtxv_maxtile,ps_video.vtxc_maxtile)<0) return -1;
    ps_video.vtxc_maxtile=0;
  }

  if (ps_video.vtxc_textile) {
    if (akgl_program_textile_draw(ps_video.program_textile,ps_video.texture_minfont,ps_video.vtxv_textile,ps_video.vtxc_textile)<0) return -1;
    ps_video.vtxc_textile=0;
  }

  return 0;
}
