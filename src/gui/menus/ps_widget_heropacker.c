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
};

#define WIDGET ((struct ps_widget_heropacker*)widget)

/* Delete.
 */

static void _ps_heropacker_del(struct ps_widget *widget) {
  ps_input_unwatch_devices(WIDGET->input_watchid);
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
  //ps_log(GUI,TRACE,"%s %d.%d=%d",__func__,plrid,btnid,value);
  //struct ps_widget *heroselect=ps_heropacker_get_heroselect_for_plrid(widget,plrid);

  if (0&&value&&(btnid==1)) {//XXX Create fake input device to test many-player layouts
    struct ps_input_device *device=ps_input_device_new(sizeof(struct ps_input_device));
    if (!device) return -1;
    if (!ps_heropacker_add_player(widget,device)) {
      ps_input_device_del(device);
      return -1;
    }
    ps_input_device_del(device);
  }
  
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

/* Add player.
 */

static struct ps_widget *ps_heropacker_add_player(struct ps_widget *widget,struct ps_input_device *device) {

  // Already have it? That's possible.
  struct ps_widget *heroselect=ps_heropacker_get_heroselect_for_device(widget,device);
  if (heroselect) return heroselect;

  // Create the new widget.
  if (!(heroselect=ps_widget_spawn(widget,&ps_widget_type_heroselect))) return 0;
  if (ps_widget_heroselect_set_device(heroselect,device)<0) return 0;
  if (ps_widget_heroselect_set_plrdefid(heroselect,1+(widget->childc%8))<0) return 0;
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
  }
  return 0;
}

static int ps_heropacker_cb_disconnect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (device->map) {
    if (ps_heropacker_remove_player(widget,device)<0) return -1;
  }
  return 0;
}
