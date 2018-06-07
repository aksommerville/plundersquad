/* ps_widget_heropacker.c
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/ps_gui.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_map.h"
#include "input/ps_input_premap.h"
#include "game/ps_game.h"
#include "game/ps_score_store.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"

#define PS_HEROPACKER_MSGBLINK_TIME_ON     40
#define PS_HEROPACKER_MSGBLINK_TIME_TOTAL  60
#define PS_HEROPACKER_ANIMATE_TIME 30

static int ps_heropacker_cb_connect(struct ps_input_device *device,void *userdata);
static int ps_heropacker_cb_disconnect(struct ps_input_device *device,void *userdata);
static struct ps_widget *ps_heropacker_add_player(struct ps_widget *widget,struct ps_input_device *device);
static struct ps_widget *ps_heropacker_get_heroselect_for_plrid(const struct ps_widget *widget,struct ps_input_device *device);

/* Object definition.
 */

struct ps_widget_heropacker {
  struct ps_widget hdr;
  int msgblinkcounter;
  int input_watchid;
  int startup;
  int *hs_usagev; // plrdefid usage from highscores
  int hs_usagec;
};

#define WIDGET ((struct ps_widget_heropacker*)widget)

/* Delete.
 */

static void _ps_heropacker_del(struct ps_widget *widget) {
  ps_input_unwatch_devices(WIDGET->input_watchid);
  if (WIDGET->hs_usagev) free(WIDGET->hs_usagev);
}

/* Initialize.
 */

static int _ps_heropacker_init(struct ps_widget *widget) {

  WIDGET->startup=1; // Stays on until the first pack.

  if ((WIDGET->input_watchid=ps_input_watch_devices(
    ps_heropacker_cb_connect,
    ps_heropacker_cb_disconnect,
    widget
  ))<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_heropacker_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_heropacker) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_heropacker_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (widget->childc>0) {
    if (ps_video_flush_cached_drawing()<0) return -1;
    if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  } else {

    WIDGET->msgblinkcounter++;
    if (WIDGET->msgblinkcounter>=PS_HEROPACKER_MSGBLINK_TIME_TOTAL) {
      WIDGET->msgblinkcounter=0;
    }

    if (WIDGET->msgblinkcounter<PS_HEROPACKER_MSGBLINK_TIME_ON) {
      const char message[]="Connect an input device.";
      int messagec=sizeof(message)-1;
      if (ps_video_text_begin()<0) return -1;
      int textsize=12;
      int x=parentx+widget->x+(widget->w>>1)-(((textsize>>1)*messagec)>>1)+(textsize>>2);
      int y=parenty+widget->y+(widget->h>>1);
      if (ps_video_text_add(textsize,0xffffffff,x,y,message,messagec)<0) return -1;
      if (ps_video_text_end(-1)<0) return -1;
    }
  }
  return 0;
}

/* Choose row count for packing.
 */

static int ps_heropacker_measure_oblongitude(int w,int h) {
  if (w>h) {
    return h*(w-h);
  } else {
    return w*(h-w);
  }
}

static int ps_heropacker_rate_row_count(const struct ps_widget *widget,int rowc) {
  int colc_base=widget->childc/rowc;
  int colc_extra=widget->childc%rowc; // So many rows get an extra column. Starting at the top.
  int rowh_base=widget->h/rowc;
  int rowh_extra=widget->h%rowc; // Bottom row has this extra height.

  /* There are up to three cell sizes:
   *   1: w/(colc_base+1),rowh_base
   *   2: w/colc_base,rowh_base
   *   3: w/colc_base,rowh_base+rowh_extra
   */
  int c1=colc_extra*(colc_base+1);
  int c3=colc_base;
  int c2=widget->childc-c1-c3;
  int w1=widget->w/(colc_base+1);
  int w23=widget->w/colc_base;
  int h12=rowh_base;
  int h3=rowh_base+rowh_extra;

  /* Score starts as the total count of pixels outside the largest square in each cell.
   */
  int score=0;
  score+=c1*ps_heropacker_measure_oblongitude(w1,h12);
  score+=c2*ps_heropacker_measure_oblongitude(w23,h12);
  score+=c3*ps_heropacker_measure_oblongitude(w23,h3);

  /* We'd prefer a uniformm layout overall, so double the score if rows have different cell counts.
   */
  if (colc_extra) score*=2;

  return score;
}

static int ps_heropacker_choose_row_count(const struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  if (widget->childc==1) return 1;
  if ((widget->w<1)||(widget->h<1)) return 1;

  /* Valid options are 1 through widget->childc.
   * We want the row count that makes children as square as possible.
   */
  int best_rowc=1;
  int best_score=INT_MAX;
  int i=1; for (;i<=widget->childc;i++) {
    int score=ps_heropacker_rate_row_count(widget,i);
    if (score<best_score) {
      best_rowc=i;
      best_score=score;
    }
  }
  return best_rowc;
}

/* Place children.
 */

static int ps_heropacker_place_children(struct ps_widget *widget,int animate) {
  if (widget->childc<1) return 0;
  int rowc=ps_heropacker_choose_row_count(widget);
  int colc_base=widget->childc/rowc;
  int colc_extra=widget->childc%rowc;
  int rowh_base=widget->h/rowc;
  int rowh_extra=widget->h%rowc;
  struct ps_gui *gui=ps_widget_get_gui(widget);

  int row=0,childp=0,y=0;
  for (;row<rowc;row++) {
    int rowh=rowh_base;
    if (row==rowc-1) rowh+=rowh_extra;
    int colc=colc_base;
    if (colc_extra) {
      colc++;
      colc_extra--;
    }
    int colw_base=widget->w/colc;
    int colw_extra=widget->w%colc;
    int col=0,x=0;
    for (;col<colc;col++,childp++) {
      struct ps_widget *child=widget->childv[childp];
      int colw=colw_base;
      if (colw_extra) {
        colw++;
        colw_extra--;
      }
      if (animate) {
        if (!child->w) { // Child never positioned -- put it in the center of the target zone.
          child->x=x+(colw>>1);
          child->y=y+(rowh>>1);
        }
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_x,x,PS_HEROPACKER_ANIMATE_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_y,y,PS_HEROPACKER_ANIMATE_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_w,colw,PS_HEROPACKER_ANIMATE_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_h,rowh,PS_HEROPACKER_ANIMATE_TIME)<0) return -1;
      } else {
        child->x=x;
        child->y=y;
        child->w=colw;
        child->h=rowh;
      }
      x+=colw;
    }
    y+=rowh;
  }
  return 0;
}

/* Pack.
 */

static int _ps_heropacker_pack(struct ps_widget *widget) {
  WIDGET->startup=0;
  if (ps_heropacker_place_children(widget,0)<0) return -1;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

static int ps_heropacker_animate_children(struct ps_widget *widget) {
  if (ps_heropacker_place_children(widget,1)<0) return -1;
  return 0;
}

/* Receive user input.
 */

static int _ps_heropacker_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_heropacker={

  .name="heropacker",
  .objlen=sizeof(struct ps_widget_heropacker),

  .del=_ps_heropacker_del,
  .init=_ps_heropacker_init,

  .draw=_ps_heropacker_draw,
  .pack=_ps_heropacker_pack,

  .userinput=_ps_heropacker_userinput,

};

/* Find child by input device.
 */
 
static struct ps_widget *ps_heropacker_get_heroselect_for_device(const struct ps_widget *widget,const struct ps_input_device *device) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *heroselect=widget->childv[i];
    if (ps_widget_heroselect_get_device(heroselect)==device) {
      return heroselect;
    }
  }
  return 0;
}

/* Select plrdefid for new heroselect.
 * Consult the resource store, highscores, and heroselects already instantiated.
 * Never fails. Returns 1 if anything goes wrong.
 */

int ps_widget_heropacker_select_plrdefid(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_heropacker)) return 1;

  /* If the restype is not available or empty, there's nothing we can do.
   */
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_PLRDEF);
  if (!restype||(restype->resc<1)) return 1;

  /* Try to acquire usage from highscores file, if we haven't done it yet. */
  if (!WIDGET->hs_usagec) {
    struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
    if (game) {
      WIDGET->hs_usagec=ps_score_store_count_plrdefid_usages(&WIDGET->hs_usagev,game->score_store);
    }
  }

  /* Compose a list for ratings, parallel to restype->resv. */
  int *ratingv=calloc(sizeof(int),restype->resc);
  if (!ratingv) return restype->resv[0].id; // There are things we could do here, but this won't happen.

  /* Lose a point for each historical usage. */
  int i; for (i=0;i<WIDGET->hs_usagec;i++) {
    int p=ps_restype_res_search(restype,WIDGET->hs_usagev[i<<1]);
    if (p<0) continue;
    ratingv[p]-=WIDGET->hs_usagev[(i<<1)+1];
  }

  /* Lose points for each child currently displaying this one, more if committed. */
  for (i=widget->childc;i-->0;) {
    struct ps_widget *heroselect=widget->childv[i];
    int plrdefid=ps_widget_heroselect_get_plrdefid(heroselect);
    if (plrdefid<0) continue;
    int p=ps_restype_res_search(restype,plrdefid);
    if (p<0) continue;
    if (ps_widget_heroselect_is_ready(heroselect)) {
      ratingv[p]-=10;
    } else {
      ratingv[p]-=5;
    }
  }

  /* Locate the highest rating, and count the entries at that rating. */
  int bestrating=INT_MIN,bestratingc=0;
  for (i=restype->resc;i-->0;) {
    if (ratingv[i]>bestrating) {
      bestrating=ratingv[i];
      bestratingc=1;
    } else if (ratingv[i]==bestrating) {
      bestratingc++;
    }
  }

  /* Select randomly among the top-rated candidates. */
  int selectionp=rand()%bestratingc;
  int plrdefid=1;
  for (i=restype->resc;i-->0;) {
    if (ratingv[i]==bestrating) {
      if (!selectionp--) {
        plrdefid=restype->resv[i].id;
        break;
      }
    }
  }

  free(ratingv);
  return plrdefid;
}

/* Add player.
 */

static struct ps_widget *ps_heropacker_add_player(struct ps_widget *widget,struct ps_input_device *device) {

  // Already have it? That's possible.
  struct ps_widget *heroselect=ps_heropacker_get_heroselect_for_device(widget,device);
  if (heroselect) return heroselect;

  // Create the new widget.
  if (!(heroselect=ps_widget_spawn(widget,&ps_widget_type_heroselect))) return 0;
  if (ps_widget_heroselect_set_device(heroselect,device)<0) return 0;
  if (ps_widget_heroselect_set_plrdefid(heroselect,1)<0) return 0; // This is a dummy until the heroselect clicks in
  if (ps_widget_heroselect_set_palette(heroselect,widget->childc-1)<0) return 0;

  // If this is the initial setup, let it roll. Otherwise, animate all children into new positions.
  if (!WIDGET->startup) {
    if (ps_heropacker_animate_children(widget)<0) return 0;
  }
  
  return heroselect;
}

/* Remove player.
 */

static int ps_heropacker_remove_player(struct ps_widget *widget,struct ps_input_device *device) {
  struct ps_widget *heroselect=ps_heropacker_get_heroselect_for_device(widget,device);
  if (!heroselect) return 0;
  if (ps_widget_remove_child(widget,heroselect)<0) return -1;
  if (!WIDGET->startup) {
    if (ps_heropacker_animate_children(widget)<0) return -1;
  }
  return 0;
}

/* Input device callbacks.
 */
 
static int ps_heropacker_cb_connect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (device->map) {
    if (!ps_heropacker_add_player(widget,device)) return -1;
  } else {
    if (ps_input_premap_build_for_device(device)<0) return -1;
    if (ps_input_premap_usable(device->premap)) {
      if (!ps_heropacker_add_player(widget,device)) return -1;
    }
  }
  return 0;
}

static int ps_heropacker_cb_disconnect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_heropacker_remove_player(widget,device)<0) return -1;
  return 0;
}

/* Count how many of my children are interacting with users.
 */
 
int ps_widget_heropacker_count_active_players(const struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_heropacker)) return 0;
  int playerc=0;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *heroselect=widget->childv[i];
    if (!heroselect||(heroselect->type!=&ps_widget_type_heroselect)) continue;
    if (ps_widget_heroselect_is_pending(heroselect)||ps_widget_heroselect_is_ready(heroselect)) {
      playerc++;
    }
  }
  return playerc;
}
