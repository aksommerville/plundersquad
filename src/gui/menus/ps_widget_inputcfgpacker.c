/* ps_widget_inputcfgpacker.c
 * Monitor input devices and create an inputcfg widget for each.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_premap.h"

#define PS_INPUTCFGPACKER_MARGIN   5
#define PS_INPUTCFGPACKER_SPACING  5
#define PS_INPUTCFGPACKER_ANIMATION_TIME 60

static int ps_inputcfgpacker_cb_connect(struct ps_input_device *device,void *userdata);
static int ps_inputcfgpacker_cb_disconnect(struct ps_input_device *device,void *userdata);

/* Object definition.
 */

struct ps_widget_inputcfgpacker {
  struct ps_widget hdr;
  int input_watchid;
  int repack;
  int animate_on_repack;
  int pvrowc;
};

#define WIDGET ((struct ps_widget_inputcfgpacker*)widget)

/* Delete.
 */

static void _ps_inputcfgpacker_del(struct ps_widget *widget) {
  ps_input_unwatch_devices(WIDGET->input_watchid);
}

/* Initialize.
 */

static int _ps_inputcfgpacker_init(struct ps_widget *widget) {

  WIDGET->animate_on_repack=0;

  if ((WIDGET->input_watchid=ps_input_watch_devices(
    ps_inputcfgpacker_cb_connect,
    ps_inputcfgpacker_cb_disconnect,
    widget
  ))<0) return -1;
  
  return 0;
}

/* Structural helpers.
 */

static int ps_inputcfgpacker_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_inputcfgpacker) return -1;
  return 0;
}

/* Select grid size based on child widget count and my dimensions.
 */

// Total area of a rectangle of (w,h), minus the largest square that fits inside it.
static int ps_inputcfgpacker_measure_outside_square(int w,int h) {
  if (w>=h) return h*(w-h);
  return w*(h-w);
}

static int ps_inputcfgpacker_rate_row_count(const struct ps_widget *widget,int rowc) {
  int availh=widget->h-(PS_INPUTCFGPACKER_MARGIN<<1)-(PS_INPUTCFGPACKER_SPACING*(rowc-1));
  int availw=widget->w-(PS_INPUTCFGPACKER_MARGIN<<1);
  int rowh_base=availh/rowc;
  int rowh_extra=availh%rowc;
  int colc_base=widget->childc/rowc;
  int colc_extra=widget->childc%rowc;

  int excess_total=0;

  int row=0,col=0;
  int colc=colc_base;
  if (colc_extra>0) {
    colc++;
    colc_extra--;
  }
  int colw_avail=availw-(PS_INPUTCFGPACKER_SPACING*(colc-1));
  int colw_base=colw_avail/colc;
  int colw_extra=colw_avail%colc;
  int rowh=rowh_base;
  if (rowh_extra>0) {
    rowh++;
    rowh_extra--;
  }
  int childp=0; for (;childp<widget->childc;childp++) {
    if (col>=colc) {
      col=0;
      colc=colc_base;
      if (colc_extra>0) {
        colc++;
        colc_extra--;
      }
      colw_avail=availw-(PS_INPUTCFGPACKER_SPACING*(colc-1));
      colw_base=colw_avail/colc;
      colw_extra=colw_avail%colc;
      row++;
      rowh=rowh_base;
      if (rowh_extra>0) {
        rowh++;
        rowh_extra--;
      }
    }
    int colw=colw_base;
    if (colw_extra>0) {
      colw++;
      colw_extra--;
    }
    excess_total+=ps_inputcfgpacker_measure_outside_square(colw,rowh);
  }

  return widget->w*widget->h-excess_total;
}

static int ps_inputcfgpacker_select_row_count(const struct ps_widget *widget) {
  if (widget->childc<1) return 0;
  int best_rowc=0,best_score=0;
  int test_rowc=1; for (;test_rowc<=widget->childc;test_rowc++) {
    int score=ps_inputcfgpacker_rate_row_count(widget,test_rowc);
    if (score>best_score) {
      best_rowc=test_rowc;
      best_score=score;
    }
  }
  if (!best_rowc) return -1;
  return best_rowc;
}

/* Set style of inputstatus.
 */
 
static int ps_inputcfgpacker_set_style(struct ps_widget *widget,int style) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    int j=child->childc; while (j-->0) {
      struct ps_widget *inputstatus=child->childv[j];
      if (inputstatus->type==&ps_widget_type_inputstatus) {
        if (ps_widget_inputstatus_set_style(inputstatus,style)<0) return -1;
      }
    }
  }
  return 0;
}

/* Pack.
 */

static int _ps_inputcfgpacker_pack(struct ps_widget *widget) {

  /* Update keyboard shortcut labels in all children. */
  int i; for (i=widget->childc;i-->0;) {
    int fkey=2+i;
    char name[4];
    if (fkey<=9) {
      name[0]='F';
      name[1]='0'+fkey;
      name[2]=0;
    } else if (fkey<=12) {
      name[0]='F';
      name[1]='1';
      name[2]='0'+fkey-10;
      name[3]=0;
    } else {
      name[0]=0;
    }
    if (ps_widget_inputcfg_set_reset_shortcut_name(widget->childv[i],name)<0) return -1;
  }

  WIDGET->repack=0;
  int rowc=ps_inputcfgpacker_select_row_count(widget);
  if (rowc<1) return 0;
  int colc_base=widget->childc/rowc;
  int colc_extra=widget->childc%rowc;

  int availh=widget->h-(PS_INPUTCFGPACKER_MARGIN<<1)-(PS_INPUTCFGPACKER_SPACING*(rowc-1));
  int rowh_base=availh/rowc;
  int rowh_extra=availh%rowc;
  
  /* If row count <=2 (<=10 devices), use the large layout.
   * >=3 rows (>=11 devices), use the small layout.
   */
  if ((WIDGET->pvrowc<3)&&(rowc>=3)) {
    if (ps_inputcfgpacker_set_style(widget,PS_WIDGET_INPUTSTATUS_STYLE_SMALL)<0) return -1;
  } else if ((WIDGET->pvrowc>=3)&&(rowc<3)) {
    if (ps_inputcfgpacker_set_style(widget,PS_WIDGET_INPUTSTATUS_STYLE_LARGE)<0) return -1;
  }

  struct ps_gui *gui=ps_widget_get_gui(widget);
  
  int childp=0;
  int y=PS_INPUTCFGPACKER_MARGIN;
  int row=0; for (;row<rowc;row++) {

    int rowh=rowh_base;
    if (rowh_extra) {
      rowh++;
      rowh_extra--;
    }
  
    int colc=colc_base;
    if (colc_extra) {
      colc++;
      colc_extra--;
    }
    if (childp>widget->childc-colc) colc=widget->childc-childp;
    if (colc<1) break;
    int availw=widget->w-(PS_INPUTCFGPACKER_MARGIN<<1)-(PS_INPUTCFGPACKER_SPACING*(colc-1));
    int colw_base=availw/colc;
    int colw_extra=availw%colc;
    
    int x=PS_INPUTCFGPACKER_MARGIN;
    int col=0; for (;col<colc;col++,childp++) {
      struct ps_widget *child=widget->childv[childp];
      int colw=colw_base;
      if (colw_extra) {
        colw++;
        colw_extra--;
      }

      if (WIDGET->animate_on_repack&&gui) {
        if (!child->w) { // Fresh child, animated from center of desired position, not top-left corner.
          child->x=x+(colw>>1);
          child->y=y+(rowh>>1);
        }
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_x,x,PS_INPUTCFGPACKER_ANIMATION_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_y,y,PS_INPUTCFGPACKER_ANIMATION_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_w,colw,PS_INPUTCFGPACKER_ANIMATION_TIME)<0) return -1;
        if (ps_gui_animate_property(gui,child,PS_WIDGET_PROPERTY_h,rowh,PS_INPUTCFGPACKER_ANIMATION_TIME)<0) return -1;
      } else {
        child->x=x;
        child->y=y;
        child->w=colw;
        child->h=rowh;
      }

      x+=colw;
      x+=PS_INPUTCFGPACKER_SPACING;
    }
    y+=rowh;
    y+=PS_INPUTCFGPACKER_SPACING;
  }
  
  for (i=0;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  WIDGET->animate_on_repack=1; // Any pack after the first one, we animate child positions.
  return 0;
}

/* Update.
 */

static int _ps_inputcfgpacker_update(struct ps_widget *widget) {
  if (WIDGET->repack) {
    if (_ps_inputcfgpacker_pack(widget)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_inputcfgpacker={

  .name="inputcfgpacker",
  .objlen=sizeof(struct ps_widget_inputcfgpacker),

  .del=_ps_inputcfgpacker_del,
  .init=_ps_inputcfgpacker_init,

  .pack=_ps_inputcfgpacker_pack,

  .update=_ps_inputcfgpacker_update,

};

/* Add widget for a device.
 */

static int ps_inputcfgpacker_add_widget_for_device(struct ps_widget *widget,struct ps_input_device *device) {

  /* Do nothing if we already have it. */
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_inputcfg_get_device(child)==device) return 0;
  }

  struct ps_widget *inputcfg=ps_widget_spawn(widget,&ps_widget_type_inputcfg);
  if (!inputcfg) return -1;
  if (ps_widget_inputcfg_setup(inputcfg,device)<0) return -1;
  WIDGET->repack=1;

  if (widget->parent) { // Don't bump the config page if we're not installed (does happen during init)
    if (ps_widget_inputcfgpage_bump(widget)<0) return -1;
  }

  return 0;
}

/* Remove widget for a device.
 */

static int ps_inputcfgpacker_remove_widget_for_device(struct ps_widget *widget,struct ps_input_device *device) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_inputcfg_get_device(child)==device) {
      if (ps_widget_remove_child(widget,child)<0) return -1;
      WIDGET->repack=1;
      if (widget->parent) {
        if (ps_widget_inputcfgpage_bump(widget)<0) return -1;
      }
      return 0;
    }
  }
  return 0;
}

/* Device connected.
 */
 
static int ps_inputcfgpacker_cb_connect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (device->map) {
    if (ps_inputcfgpacker_add_widget_for_device(widget,device)<0) return -1;
  } else {
    if (ps_input_premap_build_for_device(device)<0) return -1;
    if (ps_input_premap_usable(device->premap)) {
      if (ps_inputcfgpacker_add_widget_for_device(widget,device)<0) return -1;
    }
  }
  return 0;
}

/* Device disconnected.
 */
 
static int ps_inputcfgpacker_cb_disconnect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_inputcfgpacker_remove_widget_for_device(widget,device)<0) return -1;
  return 0;
}
