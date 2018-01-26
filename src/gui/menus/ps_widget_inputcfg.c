/* ps_widget_inputcfg.c
 * Interactive configuration for one input device.
 * Children:
 *  [0] namelabel
 *  [1] inputstatus
 *  [2] message
 *  [3] resetbutton
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "input/ps_input_map.h"
#include "input/ps_input_premap.h"
#include "input/ps_input_maptm.h"
#include "input/ps_input_config.h"

#define PS_INPUTCFG_TEXT_MARGIN 5
#define PS_INPUTCFG_MAP_COUNT 7

/* Object definition.
 */

struct ps_inputcfg_map {
  int srcbtnid;
  int srcdirection;
  uint16_t dstbtnid;
};

struct ps_widget_inputcfg {
  struct ps_widget hdr;
  struct ps_input_device *device;
  int watchid;
  
  int resetting;
  int reset_srcbtnid; // Zero or most recent mappable button press. User must press it twice in a row.
  int reset_srcdirection; // 1 or -1, in case reset_srcbtnid is a two-way axis.
  struct ps_inputcfg_map mapv[PS_INPUTCFG_MAP_COUNT]; // Temporary buffer during reset.
  int mapc;
  
};

#define WIDGET ((struct ps_widget_inputcfg*)widget)

/* Delete.
 */

static void _ps_inputcfg_del(struct ps_widget *widget) {
  if (WIDGET->device) {
    ps_input_device_unwatch_buttons(WIDGET->device,WIDGET->watchid);
    ps_input_device_del(WIDGET->device);
  }
}

/* Initialize.
 */

static int _ps_inputcfg_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0xffffffc0;
  WIDGET->watchid=-1;

  if (!(child=ps_widget_label_spawn(widget,"",0))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_inputstatus))) return -1;
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_textblock))) return -1;
  if (!(child=ps_widget_button_spawn(widget,0,"Reset",5,ps_callback(ps_inputcfg_cb_begin_reset,0,widget)))) return -1;

  WIDGET->mapv[0].dstbtnid=PS_PLRBTN_UP;
  WIDGET->mapv[1].dstbtnid=PS_PLRBTN_DOWN;
  WIDGET->mapv[2].dstbtnid=PS_PLRBTN_LEFT;
  WIDGET->mapv[3].dstbtnid=PS_PLRBTN_RIGHT;
  WIDGET->mapv[4].dstbtnid=PS_PLRBTN_A;
  WIDGET->mapv[5].dstbtnid=PS_PLRBTN_B;
  WIDGET->mapv[6].dstbtnid=PS_PLRBTN_START;
  WIDGET->mapc=0;

  return 0;
}

/* Structural helpers.
 */

static int ps_inputcfg_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_inputcfg) return -1;
  if (widget->childc!=4) return -1;
  return 0;
}

static struct ps_widget *ps_inputcfg_get_namelabel(const struct ps_widget *widget) { return widget->childv[0]; }
static struct ps_widget *ps_inputcfg_get_inputstatus(const struct ps_widget *widget) { return widget->childv[1]; }
static struct ps_widget *ps_inputcfg_get_message(const struct ps_widget *widget) { return widget->childv[2]; }
static struct ps_widget *ps_inputcfg_get_resetbutton(const struct ps_widget *widget) { return widget->childv[3]; }

/* Pack.
 */

static int _ps_inputcfg_pack(struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  struct ps_widget *namelabel=ps_inputcfg_get_namelabel(widget);
  struct ps_widget *inputstatus=ps_inputcfg_get_inputstatus(widget);
  struct ps_widget *message=ps_inputcfg_get_message(widget);
  struct ps_widget *resetbutton=ps_inputcfg_get_resetbutton(widget);
  int chw,chh;

  if (ps_widget_measure(&chw,&chh,namelabel,widget->w,widget->h)<0) return -1;
  namelabel->x=0;
  namelabel->y=0;
  namelabel->w=widget->w;
  namelabel->h=chh;

  if (ps_widget_measure(&chw,&chh,inputstatus,widget->w,widget->h-namelabel->h-namelabel->y)<0) return -1;
  inputstatus->x=0;
  inputstatus->y=namelabel->y+namelabel->h;
  inputstatus->w=widget->w;
  inputstatus->h=chh;

  if (ps_widget_measure(&chw,&chh,resetbutton,widget->w,widget->h-inputstatus->h-inputstatus->y)<0) return -1;
  resetbutton->x=(widget->w>>1)-(chw>>1);
  resetbutton->y=inputstatus->y+inputstatus->h;
  resetbutton->w=chw;
  resetbutton->h=chh;

  if (ps_widget_measure(&chw,&chh,message,widget->w-(PS_INPUTCFG_TEXT_MARGIN<<1),widget->h-resetbutton->h-resetbutton->y-(PS_INPUTCFG_TEXT_MARGIN<<1))<0) return -1;
  message->x=PS_INPUTCFG_TEXT_MARGIN;
  message->y=resetbutton->y+resetbutton->h+PS_INPUTCFG_TEXT_MARGIN;
  message->w=widget->w-(PS_INPUTCFG_TEXT_MARGIN<<1);
  message->h=chh;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_inputcfg={

  .name="inputcfg",
  .objlen=sizeof(struct ps_widget_inputcfg),

  .del=_ps_inputcfg_del,
  .init=_ps_inputcfg_init,

  .pack=_ps_inputcfg_pack,

};

/* Create input map object from our WIP.
 */

static struct ps_input_map *ps_inputcfg_new_map_from_wip(const struct ps_widget *widget) {
  struct ps_input_map *map=ps_input_map_new();
  if (!map) return 0;
  const struct ps_inputcfg_map *src=WIDGET->mapv;
  int i=PS_INPUTCFG_MAP_COUNT; for (;i-->0;src++) {

    const struct ps_input_btncfg *btncfg=ps_input_premap_get(WIDGET->device->premap,src->srcbtnid);
    if (!btncfg) {
      ps_input_map_del(map);
      return 0;
    }
    
    struct ps_input_map_fld *fld=ps_input_map_insert(map,-1,src->srcbtnid);
    if (!fld) {
      ps_input_map_del(map);
      return 0;
    }

    fld->srclo=btncfg->lo;
    fld->srchi=btncfg->hi;
    fld->dstbtnid=src->dstbtnid;

    if (btncfg->value==btncfg->lo) {
      fld->srclo=(btncfg->lo+btncfg->hi)>>1;
      if (fld->srclo==btncfg->lo) fld->srclo++;
    } else if (src->srcdirection>0) {
      fld->srclo=(btncfg->value+btncfg->hi)>>1;
      if (fld->srclo<=btncfg->value) fld->srclo=btncfg->value+1;
    } else {
      fld->srchi=(btncfg->value+btncfg->lo)>>1;
      if (fld->srchi>=btncfg->value) fld->srchi=btncfg->value-1;
    }
    
  }
  return map;
}

/* Commit WIP map to device.
 */

static int ps_inputcfg_commit_map(struct ps_widget *widget) {

  struct ps_input_map *map=ps_inputcfg_new_map_from_wip(widget);
  if (!map) return -1;
  if (!ps_input_map_can_support_player(map)) {
    ps_log(GUI,ERROR,"Somehow produced an invalid input map.");
    ps_input_map_del(map);
    return -1;
  }

  if (ps_input_device_set_map(WIDGET->device,map)<0) {
    ps_input_map_del(map);
    return -1;
  }
  ps_input_map_del(map);

  struct ps_input_config *config=ps_input_get_configuration();
  if (!config) return 0;

  struct ps_input_maptm *maptm=ps_input_maptm_generate_from_device(WIDGET->device);
  if (!maptm) {
    ps_log(GUI,ERROR,"Failed to generate input map template -- Config for device '%.*s' will not be saved.",WIDGET->device->namec,WIDGET->device->name);
    return 0;
  }

  ps_input_config_remove_similar_maptm(config,maptm);

  // Handoff maptm to global config.
  if (ps_input_config_install_maptm(config,maptm)<0) {
    ps_log(GUI,ERROR,"Failed to register input map template -- Config for device '%.*s' will not be saved.",WIDGET->device->namec,WIDGET->device->name);
    ps_input_maptm_del(maptm);
    return 0;
  }

  if (ps_input_config_save(0,config)<0) {
    ps_log(GUI,ERROR,"Failed to save input configuration.");
    return 0;
  }

  return 0;
}

/* Advance state of full reset.
 */

static int ps_inputcfg_advance_reset(struct ps_widget *widget) {

  struct ps_widget *inputstatus=ps_inputcfg_get_inputstatus(widget);

  if ((WIDGET->mapc<0)||(WIDGET->mapc>=PS_INPUTCFG_MAP_COUNT)) return -1;
  WIDGET->mapv[WIDGET->mapc].srcbtnid=WIDGET->reset_srcbtnid;
  WIDGET->mapv[WIDGET->mapc].srcdirection=WIDGET->reset_srcdirection;
  WIDGET->mapc++;

  WIDGET->reset_srcbtnid=0;

  if (WIDGET->mapc==PS_INPUTCFG_MAP_COUNT) {
    if (ps_inputcfg_commit_map(widget)<0) return -1;
    WIDGET->resetting=0;
    if (ps_widget_inputstatus_set_manual(ps_inputcfg_get_inputstatus(widget),0)<0) return -1;

  } else {
    if (ps_widget_inputstatus_highlight(inputstatus,0xffff,0)<0) return -1;
    if (ps_widget_inputstatus_highlight(inputstatus,WIDGET->mapv[WIDGET->mapc].dstbtnid,1)<0) return -1;
  }
  
  return 0;
}

/* Update message for full reset.
 */

static int ps_inputcfg_update_reset_text(struct ps_widget *widget,const char *errmsg) {
  if (!errmsg) errmsg="";

  if (WIDGET->resetting&&(WIDGET->mapc>=0)&&(WIDGET->mapc<PS_INPUTCFG_MAP_COUNT)) {
    const char *btnname=ps_plrbtn_repr(WIDGET->mapv[WIDGET->mapc].dstbtnid);
    char msg[256];
    int msgc;
    if (WIDGET->reset_srcbtnid) {
      msgc=snprintf(msg,sizeof(msg),"%sPress %s again.",errmsg,btnname);
    } else {
      msgc=snprintf(msg,sizeof(msg),"%sPlease press %s.",errmsg,btnname);
    }
    if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),msg,msgc)<0) return -1;
  } else {
    if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),"",0)<0) return -1;
  }
  
  if (ps_widget_pack(widget)<0) return -1;

  return 0;
}

/* Nonzero if this source button exists in our map already.
 */

static int ps_inputcfg_already_mapped_button(const struct ps_widget *widget,int srcbtnid,int srcdirection) {
  int i=WIDGET->mapc; while (i-->0) {
    if ((WIDGET->mapv[i].srcbtnid==srcbtnid)&&(WIDGET->mapv[i].srcdirection==srcdirection)) return 1;
  }
  return 0;
}

/* Button callback.
 */

static int ps_inputcfg_cb_button(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata) {
  struct ps_widget *widget=userdata;
  //ps_log(GUI,TRACE,"%s %d=%d [%s]",__func__,btnid,value,mapped?"mapped":"raw");

  /* Are we gathering buttons for reset? */
  if (WIDGET->resetting) {
    if (!mapped) {
      struct ps_input_btncfg *btncfg=ps_input_premap_get(device->premap,btnid);
      if (btncfg) {
        int mapped_value=0;
        int mapped_direction=1;
        if (btncfg->value==btncfg->lo) { // One-way button
          int thresh=(btncfg->lo+btncfg->hi)>>1;
          if (thresh==btncfg->lo) thresh++;
          if (value>=thresh) mapped_value=1;
        } else { // Two-way button
          if (value<btncfg->value) {
            int thresh=(btncfg->lo+btncfg->value)>>1;
            if (thresh>=btncfg->value) thresh=btncfg->value-1;
            if (value<=thresh) mapped_value=1;
            mapped_direction=-1;
          } else {
            int thresh=(btncfg->value+btncfg->hi)>>1;
            if (thresh<=btncfg->value) thresh=btncfg->value+1;
            if (value>=thresh) mapped_value=1;
          }
        }
        if (mapped_value) {
          const char *errmsg=0;
          if (!WIDGET->reset_srcbtnid) { // First stroke
            if (ps_inputcfg_already_mapped_button(widget,btnid,mapped_direction)) {
              ps_log(GUI,WARN,"Button %d already mapped, ignoring.",btnid);
              errmsg="Already in use.\n";
            } else {
              WIDGET->reset_srcbtnid=btnid;
              WIDGET->reset_srcdirection=mapped_direction;
            }
          } else if ((WIDGET->reset_srcbtnid!=btnid)||(mapped_direction!=WIDGET->reset_srcdirection)) { // Second stroke, mismatch -- reset
            WIDGET->reset_srcbtnid=0;
            errmsg="Must press twice.\n";
          } else { // Second stroke -- confirm
            if (ps_inputcfg_advance_reset(widget)<0) return -1;
          }
          if (ps_inputcfg_update_reset_text(widget,errmsg)<0) return -1;
        }
      }
    }
  }
  
  return 0;
}

/* Setup.
 */
 
int ps_widget_inputcfg_setup(struct ps_widget *widget,struct ps_input_device *device) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  if (WIDGET->device) return -1; // Already initialized.
  if (!device) return -1;
  
  if (ps_input_device_ref(device)<0) return -1;
  ps_input_device_del(WIDGET->device);
  WIDGET->device=device;
  if (ps_input_premap_build_for_device(device)<0) return -1;

  if ((WIDGET->watchid=ps_input_device_watch_buttons(device,ps_inputcfg_cb_button,0,widget))<0) return -1;

  if (ps_widget_label_set_text(ps_inputcfg_get_namelabel(widget),device->name,device->namec)<0) return -1;
  if (ps_widget_textblock_set_text(ps_inputcfg_get_message(widget),"",0)<0) return -1;
  if (ps_widget_inputstatus_set_device(ps_inputcfg_get_inputstatus(widget),device)<0) return -1;
  
  return 0;
}

/* Get device.
 */

struct ps_input_device *ps_widget_inputcfg_get_device(const struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return 0;
  return WIDGET->device;
}

/* Begin full reset.
 */
 
int ps_inputcfg_cb_begin_reset(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;

  WIDGET->resetting=1;
  WIDGET->mapc=0;
  WIDGET->reset_srcbtnid=0;
  WIDGET->reset_srcdirection=0;

  struct ps_widget *inputstatus=ps_inputcfg_get_inputstatus(widget);
  if (ps_widget_inputstatus_set_manual(inputstatus,1)<0) return -1;
  if (ps_widget_inputstatus_highlight(inputstatus,0xffff,0)<0) return -1;
  if (ps_widget_inputstatus_highlight(inputstatus,WIDGET->mapv[0].dstbtnid,1)<0) return -1;

  if (ps_inputcfg_update_reset_text(widget,0)<0) return -1;
  
  return 0;
}

/* Set keyboard shortcut name for reset button.
 */
 
int ps_widget_inputcfg_set_reset_shortcut_name(struct ps_widget *widget,const char *name) {
  if (ps_inputcfg_obj_validate(widget)<0) return -1;
  char text[32];
  int textc=snprintf(text,sizeof(text),"Reset [%s]",name);
  if ((textc<0)||(textc>sizeof(text))) textc=0;
  if (ps_widget_button_set_text(ps_inputcfg_get_resetbutton(widget),text,textc)<0) return -1;
  return 0;
}
