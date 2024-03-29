/* ps_widget_setuppage.c
 * Modal controller for the scenario generator config screen.
 * Users see this right after all players have joined.
 * When we finish, the game is ready to play.
 *
 * Children:
 *   [0] menu
 *   [1] tshirt banner -- always exists, may be no-op
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "ps_menus.h"
#include "gui/ps_gui.h"
#include "game/ps_game.h"
#include "game/ps_score_store.h"
#include "game/ps_sound_effects.h"
#include "input/ps_input.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"

/* Refuse to submit or cancel for so long after construction (microseconds).
 * This does not prevent moving the cursor.
 */
#define PS_SETUPPAGE_INITIAL_DELAY 500000

// Very important that difficulty and length here agree with gameoverpage -- otherwise we might break a promise!
#define PS_SETUPPAGE_TSHIRT_ID_NO      17 /* "Make it longer or harder." */
#define PS_SETUPPAGE_TSHIRT_ID_YES     18 /* "You can win a t-shirt!" */
#define PS_SETUPPAGE_TSHIRT_DIFFICULTY  5
#define PS_SETUPPAGE_TSHIRT_LENGTH      3

static int ps_setuppage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);
static int ps_setuppage_cb_difflen(struct ps_widget *slider,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_setuppage {
  struct ps_widget hdr;
  int64_t starttime;
  int tshirt; // <0=uninitialized. We can't check at init, must defer to the first pack.
};

#define WIDGET ((struct ps_widget_setuppage*)widget)

/* Delete.
 */

static void _ps_setuppage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_setuppage_init(struct ps_widget *widget) {
  struct ps_widget *child,*menu,*menupacker;

  widget->bgrgba=0x4080c0ff;
  WIDGET->starttime=ps_time_now();
  WIDGET->tshirt=-1;

  if (!(menu=ps_widget_spawn(widget,&ps_widget_type_menu))) return -1;
  if (!(menupacker=ps_widget_menu_get_packer(menu))) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_setuppage_cb_menu,0,widget))<0) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Begin",5)) return -1;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=0x40404080;
  if (ps_widget_slider_set_variable_color(child,0x00ff00ff,0xff0000ff)<0) return -1;
  if (ps_widget_slider_set_text(child," Difficulty",-1)<0) return -1;
  if (ps_widget_slider_set_limits(child,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX)<0) return -1;
  if (ps_widget_slider_set_value(child,PS_DIFFICULTY_DEFAULT)<0) return -1;
  if (ps_widget_slider_set_callback(child,ps_callback(ps_setuppage_cb_difflen,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=0x40404080;
  if (ps_widget_slider_set_variable_color(child,0x00ff00ff,0xff0000ff)<0) return -1;
  if (ps_widget_slider_set_text(child,"     Length",-1)<0) return -1;
  if (ps_widget_slider_set_limits(child,PS_LENGTH_MIN,PS_LENGTH_MAX)<0) return -1;
  if (ps_widget_slider_set_value(child,PS_LENGTH_DEFAULT)<0) return -1;
  if (ps_widget_slider_set_callback(child,ps_callback(ps_setuppage_cb_difflen,0,widget))<0) return -1;
  
  if (!ps_widget_menu_spawn_label(menu,"Return to player selection",-1)) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Input config",-1)) return -1;
  if (!ps_widget_menu_spawn_label(menu,"Quit",4)) return -1;
  
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_texture))) return -1;
  
  return 0;
}

/* Structural helpers.
 */

static int ps_setuppage_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_setuppage) return -1;
  if (widget->childc!=1) return -1;
  return 0;
}

static struct ps_widget *ps_setuppage_get_menu(const struct ps_widget *widget) {
  return widget->childv[0];
}

/* Populate t-shirt banner.
 */
 
static int ps_setuppage_populate_tshirt_banner(struct ps_widget *widget) {
  struct ps_widget *menu=ps_widget_menu_get_packer(widget->childv[0]);
  struct ps_widget *banner=widget->childv[1];
  if (!menu||(menu->childc<3)) return 0;
  
  int difficulty=ps_widget_slider_get_value(menu->childv[1]);
  int length=ps_widget_slider_get_value(menu->childv[2]);
  int imageid=PS_SETUPPAGE_TSHIRT_ID_NO;
  if ((difficulty>=PS_SETUPPAGE_TSHIRT_DIFFICULTY)&&(length>=PS_SETUPPAGE_TSHIRT_LENGTH)) {
    imageid=PS_SETUPPAGE_TSHIRT_ID_YES;
  }
  
  struct ps_res_IMAGE *image=ps_res_get(PS_RESTYPE_IMAGE,imageid);
  if (image) {
    if (ps_widget_texture_set_texture(banner,image->texture)<0) {
      ps_log(GUI,ERROR,"Failed to set t-shirt banner texture.");
    }
  }
  return 0;
}

/* Pack.
 */

static int _ps_setuppage_pack(struct ps_widget *widget) {
  if (widget->childc!=2) return -1;

  /* Deferred check for "tshirt" mode. */
  if (WIDGET->tshirt<0) {
    WIDGET->tshirt=ps_userconfig_get_int(ps_widget_get_userconfig(widget),"tshirt",6);
    if (WIDGET->tshirt>0) {
      if (ps_setuppage_populate_tshirt_banner(widget)<0) return -1;
    } else WIDGET->tshirt=0;
  }
  
  /* Menu gets its desired size, dead center. */
  struct ps_widget *child=widget->childv[0];
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,child,widget->w,widget->h)<0) return -1;
  child->x=(widget->w>>1)-(chw>>1);
  child->y=(widget->h>>1)-(chh>>1);
  child->w=chw;
  child->h=chh;
  if (ps_widget_pack(child)<0) return -1;
  
  /* T-shirt banner gets its desired size, centered above menu, if applicable. Otherwise zero it. */
  child=widget->childv[1];
  if (WIDGET->tshirt) {
    if (ps_widget_measure(&chw,&chh,child,widget->w,widget->childv[0]->y)<0) return -1;
    child->x=(widget->w>>1)-(chw>>1);
    child->y=(widget->childv[0]->y>>1)-(chh>>1);
    child->w=chw;
    child->h=chh;
  } else {
    child->x=child->y=child->w=child->h=0;
  }
  if (ps_widget_pack(child)<0) return -1;

  return 0;
}

/* Forward input to menu.
 */

static int _ps_setuppage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return ps_widget_userinput(ps_setuppage_get_menu(widget),plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_setuppage={

  .name="setuppage",
  .objlen=sizeof(struct ps_widget_setuppage),

  .del=_ps_setuppage_del,
  .init=_ps_setuppage_init,

  .pack=_ps_setuppage_pack,

  .userinput=_ps_setuppage_userinput,

};

/* Check time.
 * If within the initial blackout window, play a negative buzz and return nonzero.
 */
 
static int ps_setuppage_too_soon_to_leave(struct ps_widget *widget) {
  int64_t now=ps_time_now();
  if (now<WIDGET->starttime+PS_SETUPPAGE_INITIAL_DELAY) {
    //PS_SFX_GUI_REJECT
    return 1;
  }
  return 0;
}

/* Return to player select.
 */

static int ps_setuppage_return_to_assemble(struct ps_widget *widget) {
  if (ps_setuppage_too_soon_to_leave(widget)) return 0;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_widget_kill(widget)<0) return -1;
  if (ps_gui_load_page_assemble(gui)<0) return -1;
  return 0;
}

/* Read value of menu sliders.
 */

static int ps_setuppage_get_difficulty(const struct ps_widget *widget) {
  struct ps_widget *menu=ps_setuppage_get_menu(widget);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (menupacker->childc<2) return -1;
  struct ps_widget *slider=menupacker->childv[1];
  return ps_widget_slider_get_value(slider);
}

static int ps_setuppage_get_length(const struct ps_widget *widget) {
  struct ps_widget *menu=ps_setuppage_get_menu(widget);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (menupacker->childc<3) return -1;
  struct ps_widget *slider=menupacker->childv[2];
  return ps_widget_slider_get_value(slider);
}

/* Commit selections and proceed to the game.
 */

static int ps_setuppage_finish(struct ps_widget *widget) {
  if (ps_setuppage_too_soon_to_leave(widget)) return 0;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);

  if (ps_game_set_difficulty(game,ps_setuppage_get_difficulty(widget))<0) return -1;
  if (ps_game_set_length(game,ps_setuppage_get_length(widget))<0) return -1;
  if (ps_game_generate(game)<0) {
    ps_log(GUI,ERROR,"Failed to generate scenario.");
    return -1;
  }
  if (ps_game_restart(game)<0) {
    ps_log(GUI,ERROR,"Failed to start game.");
    return -1;
  }

  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Launch input configuration.
 */

static int ps_setuppage_input_config(struct ps_widget *widget) {
  return ps_gui_load_page_inputcfg(ps_widget_get_gui(widget));
}

/* Quit.
 */
 
static int ps_setuppage_quit(struct ps_widget *widget) {
  if (ps_setuppage_too_soon_to_leave(widget)) return 0;
  struct ps_userconfig *userconfig=ps_widget_get_userconfig(widget);
  if (userconfig&&ps_userconfig_get_int(userconfig,"kiosk",5)) {
    ps_log(GUI,INFO,"Ignoring quit request due to kiosk mode.");
    PS_SFX_GUI_REJECT
    return 0;
  }
  if (ps_input_request_termination()<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_setuppage_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_setuppage_finish(widget);
    case 1: break; // difficulty
    case 2: break; // length
    case 3: return ps_setuppage_return_to_assemble(widget);
    case 4: return ps_setuppage_input_config(widget);
    case 5: return ps_setuppage_quit(widget);
  }
  return 0;
}

/* Callback when difficulty or length slider changes.
 */
 
static int ps_setuppage_cb_difflen(struct ps_widget *slider,struct ps_widget *widget) {
  if (WIDGET->tshirt<1) return 0;
  if (widget->childc<2) return 0;
  if (ps_setuppage_populate_tshirt_banner(widget)<0) return -1;
  return 0;
}

/* Acquire initial settings.
 */
 
int ps_widget_setuppage_acquire_initial_settings(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_setuppage)) return -1;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);
  if (!game) return 0; // No game, just go with what we have.

  /* Ask score store for difficulty recommendation and commit it to UI if valid.
   */
  int difficulty=ps_score_store_recommend_difficulty(game->score_store,game);
  ps_log(GUI,DEBUG,"Recommended difficulty: %d",difficulty);
  if ((difficulty>=PS_DIFFICULTY_MIN)&&(difficulty<=PS_DIFFICULTY_MAX)) {
    struct ps_widget *menu=ps_setuppage_get_menu(widget);
    struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
    if (menupacker&&(menupacker->childc>=2)) {
      struct ps_widget *slider=menupacker->childv[1];
      if (ps_widget_slider_set_value(slider,difficulty)<0) return -1;
    }
  }

  /* Ask score store for the most recent length and commit it to UI if valid.
   */
  int length=ps_score_store_get_most_recent_length(game->score_store);
  ps_log(GUI,DEBUG,"Recommended length: %d",length);
  if ((length>=PS_LENGTH_MIN)&&(length<=PS_LENGTH_MAX)) {
    struct ps_widget *menu=ps_setuppage_get_menu(widget);
    struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
    if (menupacker&&(menupacker->childc>=3)) {
      struct ps_widget *slider=menupacker->childv[2];
      if (ps_widget_slider_set_value(slider,length)<0) return -1;
    }
  }
  
  if (WIDGET->tshirt>0) {
    if (ps_setuppage_populate_tshirt_banner(widget)<0) return -1;
  }
  
  return 0;
}
