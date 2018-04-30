/* ps_widget_awards.c
 * Displays each hero and the award he earned, for the game over report.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "game/ps_game.h"
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "util/ps_enums.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define PS_AWARDS_TILE_SIZE 16
#define PS_AWARDS_TEXT_HEIGHT 12
#define PS_AWARDS_TEXT_COLOR 0x002040ff

/* Object definition.
 */

struct ps_awards_player {
  int award;
  uint8_t tileid;
  uint32_t rgba;
  const char *desc;
  int descc;
  int playerid;
};

struct ps_widget_awards {
  struct ps_widget hdr;
  struct ps_awards_player playerv[PS_PLAYER_LIMIT];
  int playerc;
  struct akgl_vtx_maxtile vtxv_maxtile[PS_PLAYER_LIMIT];
};

#define WIDGET ((struct ps_widget_awards*)widget)

/* Delete.
 */

static void _ps_awards_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_awards_init(struct ps_widget *widget) {
  return 0;
}

/* Draw text.
 */

static int ps_awards_draw_text(struct ps_widget *widget,int offx,int offy) {

  if (ps_video_text_begin()<0) return -1;

  int y=offy+(PS_AWARDS_TEXT_HEIGHT>>1);
  if (PS_AWARDS_TEXT_HEIGHT<PS_AWARDS_TILE_SIZE) {
    y+=(PS_AWARDS_TILE_SIZE-PS_AWARDS_TEXT_HEIGHT)>>1;
  }
  struct ps_awards_player *player=WIDGET->playerv;
  int i=0; for (;i<WIDGET->playerc;i++,player++,y+=PS_AWARDS_TEXT_HEIGHT) {
    int x;
    if (i&1) { // align to right
      x=widget->w-PS_AWARDS_TILE_SIZE;
      x-=player->descc*(PS_AWARDS_TEXT_HEIGHT>>1);
    } else { // align to left
      x=PS_AWARDS_TILE_SIZE;
      x+=PS_AWARDS_TEXT_HEIGHT>>1;
    }
    x+=offx;
    if (ps_video_text_add(PS_AWARDS_TEXT_HEIGHT,PS_AWARDS_TEXT_COLOR,x,y,player->desc,player->descc)<0) return -1;
  }

  if (ps_video_text_end(0)<0) return -1;
  
  return 0;
}

/* Draw tiles.
 */

static int ps_awards_draw_tiles(struct ps_widget *widget,int offx,int offy) {
  int y0=offy+(PS_AWARDS_TEXT_HEIGHT>>1);
  if (PS_AWARDS_TEXT_HEIGHT<PS_AWARDS_TILE_SIZE) {
    y0+=(PS_AWARDS_TILE_SIZE-PS_AWARDS_TEXT_HEIGHT)>>1;
  }
    
  struct akgl_vtx_maxtile *vtx=WIDGET->vtxv_maxtile;
  struct ps_awards_player *player=WIDGET->playerv;
  int i=0; for (;i<WIDGET->playerc;i++,vtx++,player++) {
  
    int y=y0+i*PS_AWARDS_TEXT_HEIGHT;
    int x=offx;
    if (i&1) { // right
      x+=widget->w-(PS_AWARDS_TILE_SIZE>>1);
    } else { // left
      x+=PS_AWARDS_TILE_SIZE>>1;
    }

    vtx->x=x;
    vtx->y=y;
    vtx->size=PS_AWARDS_TILE_SIZE;
    vtx->tileid=player->tileid;
    vtx->ta=0;
    vtx->pr=player->rgba>>24;
    vtx->pg=player->rgba>>16;
    vtx->pb=player->rgba>>8;
    vtx->a=0xff;
    vtx->t=0;
    vtx->xform=AKGL_XFORM_NONE;

  }
  if (ps_video_draw_maxtile(WIDGET->vtxv_maxtile,WIDGET->playerc,0x03)<0) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_awards_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (WIDGET->playerc) {
    if (ps_awards_draw_text(widget,parentx+widget->x,parenty+widget->y)<0) return -1;
    if (ps_awards_draw_tiles(widget,parentx+widget->x,parenty+widget->y)<0) return -1;
  }
  return 0;
}

/* Measure.
 */

static int _ps_awards_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=0;
  *h=0;
  if (WIDGET->playerc) {

    *h=PS_AWARDS_TEXT_HEIGHT*WIDGET->playerc;
    if (PS_AWARDS_TILE_SIZE>PS_AWARDS_TEXT_HEIGHT) {
      (*h)+=PS_AWARDS_TILE_SIZE-PS_AWARDS_TEXT_HEIGHT;
    }
    
    int i=WIDGET->playerc; while (i-->0) {
      if (WIDGET->playerv[i].descc>*w) *w=WIDGET->playerv[i].descc;
    }
    (*w)*=PS_AWARDS_TEXT_HEIGHT>>1;
    (*w)+=PS_AWARDS_TILE_SIZE<<1;

  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_awards={

  .name="awards",
  .objlen=sizeof(struct ps_widget_awards),

  .del=_ps_awards_del,
  .init=_ps_awards_init,

  .draw=_ps_awards_draw,
  .measure=_ps_awards_measure,

};

/* Setup from game.
 */
 
int ps_widget_awards_setup(struct ps_widget *widget,const struct ps_game *game) {
  if (!widget||(widget->type!=&ps_widget_type_awards)) return -1;
  if (!game) {
    WIDGET->playerc=0;
  } else {
    struct ps_awards_player *player=WIDGET->playerv;
    int i=0; for (;i<game->playerc;i++,player++) {
      struct ps_player *src=game->playerv[i];
      if (!src) return -1;
      player->playerid=i+1;
      player->award=src->award;
      if (!(player->desc=ps_award_describe(player->award))) player->desc="?";
      player->descc=0;
      while (player->desc[player->descc]) player->descc++;
      player->tileid=src->plrdef->tileid_head;
      player->rgba=src->plrdef->palettev[src->palette].rgba_head;
    }
    WIDGET->playerc=game->playerc;
  }
  return 0;
}
