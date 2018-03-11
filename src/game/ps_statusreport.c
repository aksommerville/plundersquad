#include "ps.h"
#include "ps_statusreport.h"
#include "ps_game.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_blueprint.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"

#define PS_STATUSREPORT_COLC_MIN   4
#define PS_STATUSREPORT_COLC_MAX   4
#define PS_STATUSREPORT_ROWC_MIN   3
#define PS_STATUSREPORT_ROWC_MAX   3
#define PS_STATUSREPORT_MARGIN_LEFT   4
#define PS_STATUSREPORT_MARGIN_RIGHT  4
#define PS_STATUSREPORT_MARGIN_TOP    4
#define PS_STATUSREPORT_MARGIN_BOTTOM 8
#define PS_STATUSREPORT_TREASURELIST_HEIGHT 6

#define PS_STATUSREPORT_UNVISITED_COLOR  0x808080c0
#define PS_STATUSREPORT_EDGE_COLOR       0x000000ff
#define PS_STATUSREPORT_TREASURE_COLOR   0xff0000ff
#define PS_STATUSREPORT_COLLECTED_COLOR  0xa04040ff
#define PS_STATUSREPORT_LISTSEP_COLOR    0x000000ff

/* On the easiest settings, we draw the "full map", with screen edges and treasure locations marked.
 * Intermediate settings get the "partial map" with no treasure locations.
 * Hardest settings get no map, just a list of treasures.
 */
#define PS_STATUSREPORT_MAX_DIFFICULTY_FOR_FULL_MAP    3
#define PS_STATUSREPORT_MAX_DIFFICULTY_FOR_PARTIAL_MAP 6

/* Object lifecycle.
 */
 
struct ps_statusreport *ps_statusreport_new() {
  struct ps_statusreport *report=calloc(1,sizeof(struct ps_statusreport));
  if (!report) return 0;
  report->refc=1;
  return report;
}

void ps_statusreport_del(struct ps_statusreport *report) {
  if (!report) return;
  if (report->refc-->1) return;

  akgl_texture_del(report->texture);
  
  free(report);
}

int ps_statusreport_ref(struct ps_statusreport *report) {
  if (!report) return -1;
  if (report->refc<1) return -1;
  if (report->refc==INT_MAX) return -1;
  report->refc++;
  return 0;
}

/* Set pixels in an RGBA image.
 * WARNING: Caller must provide valid coordinates. We don't clip or anything.
 */

static void ps_statusreport_set_pixels(uint8_t *pixels,int stride,int x,int y,int w,int h,uint32_t rgba) {
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  pixels+=y*stride;
  pixels+=4*x;
  for (;h-->0;pixels+=stride) {
    uint8_t *dst=pixels;
    int i=w; for (;i-->0;dst+=4) {
      dst[0]=r;
      dst[1]=g;
      dst[2]=b;
      dst[3]=a;
    }
  }
}

/* Draw one screen into the buffer.
 * draw_treasure:
 *   0 = none
 *   1 = collected
 *   2 = uncollected
 */

static int ps_statusreport_draw_screen(uint8_t *pixels,int stride,int x,int y,int w,int h,const struct ps_screen *screen,int draw_treasure) {
  if ((w<3)||(h<3)) return -1;

  /* Fill with unvisited color if the player hasn't seen this grid yet.
   * Otherwise accept the default (fully transparent).
   */
  if (!screen->grid->visited) {
    ps_statusreport_set_pixels(pixels,stride,x,y,w,h,PS_STATUSREPORT_UNVISITED_COLOR);
  }

  /* Blacken the four corners. */
  ps_statusreport_set_pixels(pixels,stride,x,y,1,1,PS_STATUSREPORT_EDGE_COLOR);
  ps_statusreport_set_pixels(pixels,stride,x+w-1,y,1,1,PS_STATUSREPORT_EDGE_COLOR);
  ps_statusreport_set_pixels(pixels,stride,x,y+h-1,1,1,PS_STATUSREPORT_EDGE_COLOR);
  ps_statusreport_set_pixels(pixels,stride,x+w-1,y+h-1,1,1,PS_STATUSREPORT_EDGE_COLOR);

  /* If any edge is closed, blacken it. */
  if (screen->doorn==PS_DOOR_CLOSED) ps_statusreport_set_pixels(pixels,stride,x+1,y,w-2,1,PS_STATUSREPORT_EDGE_COLOR);
  if (screen->doors==PS_DOOR_CLOSED) ps_statusreport_set_pixels(pixels,stride,x+1,y+h-1,w-2,1,PS_STATUSREPORT_EDGE_COLOR);
  if (screen->doorw==PS_DOOR_CLOSED) ps_statusreport_set_pixels(pixels,stride,x,y+1,1,h-2,PS_STATUSREPORT_EDGE_COLOR);
  if (screen->doore==PS_DOOR_CLOSED) ps_statusreport_set_pixels(pixels,stride,x+w-1,y+1,1,h-2,PS_STATUSREPORT_EDGE_COLOR);

  /* Draw the treasure indicator if requested. */
  if (draw_treasure) {
    int tw=w/3;
    int tx=x+tw;
    int th=h/3;
    int ty=y+th;
    uint32_t treasurecolor;
    if (draw_treasure==2) treasurecolor=PS_STATUSREPORT_TREASURE_COLOR;
    else treasurecolor=PS_STATUSREPORT_COLLECTED_COLOR;
    ps_statusreport_set_pixels(pixels,stride,tx,ty,tw,th,treasurecolor);
  }
  
  return 0;
}

/* Draw the full-bells-and-whistles map.
 */

static int ps_statusreport_compose_image_full_map(void *pixels,int w,int h,const struct ps_statusreport *report,const struct ps_game *game) {

  int screenw_base=w/game->scenario->w;
  int screenh_base=h/game->scenario->h;
  int screenw_extra=w%game->scenario->w;
  int screenh_extra=h%game->scenario->h;
  int stride=w<<2;

  const struct ps_screen *screen=game->scenario->screenv;
  int y=0;
  int row=0; for (;row<game->scenario->h;row++) {
    int rowh=screenh_base;
    if (row<screenh_extra) rowh++;
    int x=0;
    int col=0; for (;col<game->scenario->w;col++,screen++) {
      int colw=screenw_base;
      if (col<screenw_extra) colw++;

      int draw_treasure=0;
      const struct ps_blueprint_poi *poi=screen->grid->poiv;
      int i=screen->grid->poic; for (;i-->0;poi++) {
        if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
          draw_treasure=1;
          if ((poi->argv[0]>=0)&&(poi->argv[0]<PS_TREASURE_LIMIT)) {
            if (!game->treasurev[poi->argv[0]]) draw_treasure=2;
          }
          break;
        }
      }
      
      if (ps_statusreport_draw_screen(pixels,stride,x,y,colw,rowh,screen,draw_treasure)<0) return -1;
      x+=colw;
    }
    y+=rowh;
  }
  
  return 0;
}

/* Draw the map without treasure locations.
 */

static int ps_statusreport_compose_image_partial_map(void *pixels,int w,int h,const struct ps_statusreport *report,const struct ps_game *game) {

  int listh=PS_STATUSREPORT_TREASURELIST_HEIGHT;
  int maph=h-listh;

  int screenw_base=w/game->scenario->w;
  int screenh_base=maph/game->scenario->h;
  int screenw_extra=w%game->scenario->w;
  int screenh_extra=maph%game->scenario->h;
  int listw_base=w/game->treasurec;
  int listw_extra=w%game->treasurec;
  int stride=w<<2;

  const struct ps_screen *screen=game->scenario->screenv;
  int y=0;
  int row=0; for (;row<game->scenario->h;row++) {
    int rowh=screenh_base;
    if (row<screenh_extra) rowh++;
    int x=0;
    int col=0; for (;col<game->scenario->w;col++,screen++) {
      int colw=screenw_base;
      if (col<screenw_extra) colw++;
      if (ps_statusreport_draw_screen(pixels,stride,x,y,colw,rowh,screen,0)<0) return -1;
      x+=colw;
    }
    y+=rowh;
  }

  int x=0,i=0;
  for (;i<game->treasurec;i++) {
    int colw=listw_base;
    if (i<listw_extra) colw++;
    uint32_t rgba=(game->treasurev[i]?PS_STATUSREPORT_COLLECTED_COLOR:PS_STATUSREPORT_TREASURE_COLOR);
    ps_statusreport_set_pixels(pixels,stride,x,maph,colw,listh,rgba);
    if (i>0) {
      ps_statusreport_set_pixels(pixels,stride,x,maph,1,listh,PS_STATUSREPORT_LISTSEP_COLOR);
    }
    x+=colw;
  }
  
  return 0;
}

/* Draw the list of treasures.
 */

static int ps_statusreport_compose_image_treasure_only(void *pixels,int w,int h,const struct ps_statusreport *report,const struct ps_game *game) {
  int stride=w<<2;

  /* Calculate our layout for every possible arrangement. (there are not very many)
   */
  struct layout {
    int colc,rowc;
    int emptyc;
    int colw,rowh;
    int colwx,rowhx;
    int score;
  } layoutv[PS_TREASURE_LIMIT];
  int layoutc=0;
  struct layout *layout=layoutv;
  struct layout *bestlayout=layout;
  while (layoutc<game->treasurec) {
    layout->colc=layoutc+1;
    layout->rowc=game->treasurec/layout->colc;
    int extra=game->treasurec%layout->colc;
    if (extra) {
      layout->rowc++;
      layout->emptyc=layout->colc-extra;
    } else {
      layout->emptyc=0;
    }
    layout->colw=w/layout->colc;
    layout->colwx=w%layout->colc;
    layout->rowh=h/layout->rowc;
    layout->rowhx=h%layout->rowc;

    /* Score starts as negative difference between column width and height. */
    if (layout->colw>layout->rowh) {
      layout->score=layout->rowh-layout->colw;
    } else {
      layout->score=layout->colw-layout->rowh;
    }
    /* Lose 5 points for each empty cell. */
    layout->score-=layout->emptyc*5;

    if (layout->score>bestlayout->score) {
      bestlayout=layout;
    }
    
    layout++;
    layoutc++;
  }
  layout=bestlayout;

  /* Draw a cell for each treasure. */
  int y=0,treasurep=0;
  int row=0; for (;row<layout->rowc;row++) {
    int rowh=layout->rowh;
    if (row<layout->rowhx) rowh++;
    int x=0,col=0; for (;col<layout->colc;col++,treasurep++) {
      if (treasurep>=game->treasurec) break;
      int colw=layout->colw;
      if (col<layout->colwx) colw++;
      uint32_t color=(game->treasurev[treasurep]?PS_STATUSREPORT_COLLECTED_COLOR:PS_STATUSREPORT_TREASURE_COLOR);
      ps_statusreport_set_pixels(pixels,stride,x,y,colw,rowh,color);
      if (col) ps_statusreport_set_pixels(pixels,stride,x,y,1,rowh,PS_STATUSREPORT_LISTSEP_COLOR);
      if (row) ps_statusreport_set_pixels(pixels,stride,x,y,colw,1,PS_STATUSREPORT_LISTSEP_COLOR);
      x+=colw;
    }
    y+=rowh;
  }

  return 0;
}

/* Compose image. Output is packed RGBA, initially zeroed.
 */

static int ps_statusreport_compose_image(void *pixels,int w,int h,const struct ps_statusreport *report,const struct ps_game *game) {

  // <1 treasure can only happen with a test scenario; just ignore it.
  if (game->treasurec<1) return 0;

  if (game->difficulty<=PS_STATUSREPORT_MAX_DIFFICULTY_FOR_FULL_MAP) {
    return ps_statusreport_compose_image_full_map(pixels,w,h,report,game);
  }

  if (game->difficulty<=PS_STATUSREPORT_MAX_DIFFICULTY_FOR_PARTIAL_MAP) {
    return ps_statusreport_compose_image_partial_map(pixels,w,h,report,game);
  }

  return ps_statusreport_compose_image_treasure_only(pixels,w,h,report,game);
}

/* Set up.
 */
 
int ps_statusreport_setup(struct ps_statusreport *report,const struct ps_game *game,int x,int y,int w,int h) {
  if (!report||!game) return -1;
  ps_log(GAME,TRACE,"%s (%d,%d,%d,%d)",__func__,x,y,w,h);

  if (
    (w<PS_STATUSREPORT_COLC_MIN)||(w>PS_STATUSREPORT_COLC_MAX)||
    (h<PS_STATUSREPORT_ROWC_MIN)||(h>PS_STATUSREPORT_ROWC_MAX)
  ) {
    ps_log(GAME,ERROR,"Output size %dx%d cells not acceptable for statusreport.",w,h);
    return -1;
  }

  int imgw=w*PS_TILESIZE-PS_STATUSREPORT_MARGIN_LEFT-PS_STATUSREPORT_MARGIN_RIGHT;
  int imgh=h*PS_TILESIZE-PS_STATUSREPORT_MARGIN_TOP-PS_STATUSREPORT_MARGIN_BOTTOM;
  void *pixels=calloc(4,imgw*imgh);
  if (!pixels) return -1;

  if (ps_statusreport_compose_image(pixels,imgw,imgh,report,game)<0) {
    free(pixels);
    return -1;
  }

  if (report->texture) akgl_texture_del(report->texture);
  if (!(report->texture=akgl_texture_new())) {
    free(pixels);
    return -1;
  }

  if (akgl_texture_load(report->texture,pixels,AKGL_FMT_RGBA8,imgw,imgh)<0) {
    free(pixels);
    return -1;
  }

  free(pixels);

  report->dstx=x*PS_TILESIZE+PS_STATUSREPORT_MARGIN_LEFT;
  report->dsty=y*PS_TILESIZE+PS_STATUSREPORT_MARGIN_TOP;
  report->dstw=imgw;
  report->dsth=imgh;
  
  return 0;
}

/* Draw.
 */

int ps_statusreport_draw(struct ps_statusreport *report,int offx,int offy) {
  if (!report) return -1;
  return ps_video_draw_texture(report->texture,report->dstx+offx,report->dsty+offy,report->dstw,report->dsth);
}
