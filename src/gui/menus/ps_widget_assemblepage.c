/* ps_widget_assemblepage.c
 * Controller for main menu, first thing the user sees.
 *
 * Children:
 *  [0] titlelabel
 *  [1] heropacker
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "../corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "input/ps_input_device.h"

// Checking the ready state might be a little expensive, so only do it once per 15 frames.
// That is desirable anyway, since we want a brief oh-shit delay after all players are ready.
#define PS_ASSEMBLEPAGE_READY_DELAY 15
#define PS_ASSEMBLEPAGE_READY_DELAY_FINAL 40

static int ps_assemblepage_finish(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_assemblepage {
  struct ps_widget hdr;
  int readydelay;
  int ready;
};

#define WIDGET ((struct ps_widget_assemblepage*)widget)

/* Delete.
 */

static void _ps_assemblepage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_assemblepage_init(struct ps_widget *widget) {

  widget->bgrgba=0x004090ff;

  struct ps_widget *child;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_label))) return -1; // titlelabel
  if (ps_widget_label_set_text(child,"Plunder Squad",-1)<0) return -1;
  if (ps_widget_label_set_size(child,24)<0) return -1;
  child->fgrgba=0xffffffff;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_heropacker))) return -1; // heropacker

  return 0;
}

/* Structural helpers.
 */

static int ps_assemblepage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_assemblepage) return -1;
  if (widget->childc!=2) return -1;
  return 0;
}

static struct ps_widget *ps_assemblepage_get_titlelabel(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_assemblepage_get_heropacker(const struct ps_widget *widget) {
  return widget->childv[1];
}

/* Pack.
 */

static int _ps_assemblepage_pack(struct ps_widget *widget) {
  if (ps_assemblepage_obj_validate(widget)<0) return -1;
  struct ps_widget *titlelabel=ps_assemblepage_get_titlelabel(widget);
  struct ps_widget *heropacker=ps_assemblepage_get_heropacker(widget);
  int chw,chh;

  /* titlelabel gets double its preferred height, and the full width. */
  if (ps_widget_measure(&chw,&chh,titlelabel,widget->w,widget->h)<0) return -1;
  titlelabel->x=0;
  titlelabel->y=0;
  titlelabel->w=widget->w;
  titlelabel->h=chh<<1;

  /* heropacker gets the rest. */
  heropacker->x=0;
  heropacker->y=titlelabel->y+titlelabel->h;
  heropacker->w=widget->w;
  heropacker->h=widget->h-heropacker->y;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Poll user readiness.
 */

static int ps_assemblepage_poll_ready(struct ps_widget *widget) {
  if (ps_assemblepage_obj_validate(widget)<0) return -1;
  struct ps_widget *heropacker=ps_assemblepage_get_heropacker(widget);
  int readyc=0;
  int i=heropacker->childc; while (i-->0) {
    struct ps_widget *heroselect=heropacker->childv[i];
    if (ps_widget_heroselect_is_pending(heroselect)) return 0;
    if (ps_widget_heroselect_is_ready(heroselect)) readyc++;
  }
  if (readyc) return 1;
  return 0;
}

/* Update.
 */

static int _ps_assemblepage_update(struct ps_widget *widget) {
  int i=widget->childc; while (i-->0) {
    if (ps_widget_update(widget->childv[i])<0) return -1;
  }

  WIDGET->readydelay--;
  if (WIDGET->readydelay<=0) {
    int ready=ps_assemblepage_poll_ready(widget);
    if (ready) {
      if (WIDGET->ready) {
        return ps_assemblepage_finish(widget);
      } else {
        WIDGET->ready=1;
        WIDGET->readydelay=PS_ASSEMBLEPAGE_READY_DELAY_FINAL;
      }
    } else {
      WIDGET->ready=0;
      WIDGET->readydelay=PS_ASSEMBLEPAGE_READY_DELAY;
    }
  }
  
  return 0;
}

/* Receive and dispatch input.
 */

static int _ps_assemblepage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  struct ps_widget *heropacker=ps_assemblepage_get_heropacker(widget);
  return ps_widget_userinput(heropacker,plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_assemblepage={

  .name="assemblepage",
  .objlen=sizeof(struct ps_widget_assemblepage),

  .del=_ps_assemblepage_del,
  .init=_ps_assemblepage_init,

  .pack=_ps_assemblepage_pack,

  .update=_ps_assemblepage_update,
  .userinput=_ps_assemblepage_userinput,

};

/* Find the game, gather our changes, and commit them.
 */

static int ps_assemblepage_commit_to_game(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);
  struct ps_widget *heropacker=ps_assemblepage_get_heropacker(widget);
  int i;

  struct ps_plrcfg {
    int plrdefid;
    int palette;
    struct ps_input_device *device;
  } playerv[PS_PLAYER_LIMIT];

  int playerc=0;
  for (i=heropacker->childc;i-->0;) {
    struct ps_widget *heroselect=heropacker->childv[i];
    if (ps_widget_heroselect_is_ready(heroselect)) {
      if (playerc>=PS_PLAYER_LIMIT) {
        struct ps_input_device *device=ps_widget_heroselect_get_device(heroselect);
        ps_log(GUI,ERROR,"Ignoring player plrdef:%d on device '%.*s'; 8 players already selected.",
          ps_widget_heroselect_get_plrdefid(heroselect),
          device?device->namec:0,device?device->name:""
        );
      } else {
        struct ps_plrcfg *plrcfg=playerv+playerc++;
        plrcfg->plrdefid=ps_widget_heroselect_get_plrdefid(heroselect);
        plrcfg->palette=ps_widget_heroselect_get_palette(heroselect);
        plrcfg->device=ps_widget_heroselect_get_device(heroselect);
      }
    }
  }

  if (!playerc) {
    ps_log(GUI,ERROR,"Assembly page terminated with no players selected.");
    return -1;
  }

  if (ps_game_set_player_count(game,playerc)<0) return -1;
  for (i=0;i<playerc;i++) {
    if (ps_game_configure_player(game,i+1,playerv[i].plrdefid,playerv[i].palette,playerv[i].device)<0) {
      ps_log(GUI,ERROR,
        "Failed to configure player %d (plrdefid=%d, palette=%d, device=%p)",
        i+1,playerv[i].plrdefid,playerv[i].palette,playerv[i].device
      );
      return -1;
    }
  }
  
  return 0;
}

/* Commit changes and advance UI state.
 */
 
static int ps_assemblepage_finish(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_assemblepage_commit_to_game(widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  if (ps_gui_load_page_setup(gui)<0) return -1;
  return 0;
}
