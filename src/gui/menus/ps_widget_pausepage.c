/* ps_widget_pausepage.c
 * Menu for special actions during game play.
 * We track input for a cheat code to open the debug menu (LLLRRRLR).
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "game/ps_game.h"
#include "game/ps_switchboard.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "video/ps_video.h"

#define PS_PAUSEPAGE_CHEAT_LIMIT 8
#define PS_PAUSEPAGE_CHEAT_TIME  2000000

static int ps_pausepage_menucb_resume(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_to_beginning(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_restart(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_end_game(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_input(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_audio(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_fullscreen(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_quit(struct ps_widget *button,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_pausepage {
  struct ps_widget hdr;
  int cheat_btnidv[PS_PAUSEPAGE_CHEAT_LIMIT];
  int cheat_btnidp;
  int64_t cheat_expiration;
};

#define WIDGET ((struct ps_widget_pausepage*)widget)

/* Delete.
 */

static void _ps_pausepage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_pausepage_init(struct ps_widget *widget) {

  widget->bgrgba=0x000000c0;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (!menupacker) return -1;

  struct ps_widget *thumb=ps_widget_menu_get_thumb(menu);
  if (!thumb) return -1;
  thumb->bgrgba=0x206080ff;

  #define MENUITEM(label,cbtag) { \
    struct ps_widget *item=ps_widget_menu_spawn_button(menu,label,sizeof(label)-1,ps_callback(ps_pausepage_menucb_##cbtag,0,widget)); \
    if (!item) return -1; \
    if (ps_widget_button_set_text_color(item,0xffffffff)<0) return -1; \
  }
  MENUITEM("Resume",resume)
  MENUITEM("To beginning",to_beginning)
  MENUITEM("Restart",restart)
  MENUITEM("End game",end_game)
  MENUITEM("Input",input)
  MENUITEM("Audio",audio)
  if (ps_video_supports_fullscreen_toggle()) {
    MENUITEM("Toggle fullscreen",fullscreen)
  }
  MENUITEM("Quit",quit)
  #undef MENUITEM
  
  return 0;
}

/* Pack.
 */

static int _ps_pausepage_pack(struct ps_widget *widget) {
  if (widget->childc!=1) return -1;
  struct ps_widget *menu=widget->childv[0];
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,menu,widget->w,widget->h)<0) return -1;
  menu->x=(widget->w>>1)-(chw>>1);
  menu->y=(widget->h>>1)-(chh>>1);
  menu->w=chw;
  menu->h=chh;
  if (ps_widget_pack(menu)<0) return -1;
  return 0;
}

/* Open the cheat menu.
 */

static int ps_pausepage_open_cheat_menu(struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_gui_load_page_debugmenu(gui)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Clear the cheat code.
 */

static int ps_pausepage_clear_cheat_code(struct ps_widget *widget) {
  WIDGET->cheat_btnidp=0;
  memset(WIDGET->cheat_btnidv,0,sizeof(WIDGET->cheat_btnidv));
  return 0;
}

/* Add a keystroke to the cheat code buffer.
 */

static int ps_pausepage_append_cheat_code(struct ps_widget *widget,int btnid) {
  WIDGET->cheat_btnidv[WIDGET->cheat_btnidp]=btnid;
  if (++(WIDGET->cheat_btnidp)>=PS_PAUSEPAGE_CHEAT_LIMIT) WIDGET->cheat_btnidp=0;
  return 0;
}

/* Test the cheat code buffer, return nonzero if our secret code is present.
 */

static inline int ps_pausepage_cheat_code_equals(const struct ps_widget *widget,unsigned int p,int btnid) {
  p+=WIDGET->cheat_btnidp;
  p%=PS_PAUSEPAGE_CHEAT_LIMIT;
  return (WIDGET->cheat_btnidv[p]==btnid);
}

static int ps_pausepage_cheat_code_present(const struct ps_widget *widget) {
  if (!ps_pausepage_cheat_code_equals(widget,0,PS_PLRBTN_LEFT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,1,PS_PLRBTN_LEFT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,2,PS_PLRBTN_LEFT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,3,PS_PLRBTN_RIGHT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,4,PS_PLRBTN_RIGHT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,5,PS_PLRBTN_RIGHT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,6,PS_PLRBTN_LEFT)) return 0;
  if (!ps_pausepage_cheat_code_equals(widget,7,PS_PLRBTN_RIGHT)) return 0;
  return 1;
}

/* Add a keystroke to the cheat tracker and take any warranted action.
 * Returns >0 if we consumed the event (don't pass it on to the menu).
 */

static int ps_pausepage_check_cheat_code(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (!value) return 0;
  switch (btnid) {
    case PS_PLRBTN_LEFT:
    case PS_PLRBTN_RIGHT:
      break;
    default: {
        if (ps_pausepage_clear_cheat_code(widget)<0) return -1;
        return 0;
      }
  }

  if (WIDGET->cheat_expiration) {
    int64_t now=ps_time_now();
    if (now>WIDGET->cheat_expiration) {
      if (ps_pausepage_clear_cheat_code(widget)<0) return -1;
      WIDGET->cheat_expiration=now+PS_PAUSEPAGE_CHEAT_TIME;
    }
  } else {
    WIDGET->cheat_expiration=ps_time_now()+PS_PAUSEPAGE_CHEAT_TIME;
  }
  
  if (ps_pausepage_append_cheat_code(widget,btnid)<0) return -1;
  if (ps_pausepage_cheat_code_present(widget)) {
    if (ps_pausepage_open_cheat_menu(widget)<0) return -1;
  }
  return 1;
}

/* Input.
 */

static int _ps_pausepage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (widget->childc!=1) return -1;
  int err=ps_pausepage_check_cheat_code(widget,plrid,btnid,value);
  if (err) return err;
  return ps_widget_userinput(widget->childv[0],plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_pausepage={

  .name="pausepage",
  .objlen=sizeof(struct ps_widget_pausepage),

  .del=_ps_pausepage_del,
  .init=_ps_pausepage_init,

  .pack=_ps_pausepage_pack,

  .userinput=_ps_pausepage_userinput,

};

/* Dismiss menu.
 */

static int ps_pausepage_menucb_resume(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Go to beginning of game.
 */

static int ps_pausepage_menucb_to_beginning(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_return_to_start_screen(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Restart game.
 */

static int ps_pausepage_menucb_restart(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_restart(game)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Cancel game, return to menu.
 */

static int ps_pausepage_menucb_end_game(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_assemble(gui)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Configure input.
 */

static int ps_pausepage_menucb_input(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_inputcfg(gui)<0) return -1;
  return 0;
}

/* Configure audio.
 */

static int ps_pausepage_menucb_audio(struct ps_widget *button,struct ps_widget *widget) {
  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (ps_gui_load_page_audiocfg(gui)<0) return -1;
  return 0;
}

/* Toggle fullscreen.
 */
 
static int ps_pausepage_menucb_fullscreen(struct ps_widget *button,struct ps_widget *widget) {
  int fullscreen=ps_video_toggle_fullscreen();
  if (fullscreen<0) return -1;

  struct ps_userconfig *userconfig=ps_widget_get_userconfig(widget);
  if (userconfig) {
    if (ps_userconfig_set_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"fullscreen",10),fullscreen)>=0) {
    }
  }
  
  return 0;
}

/* Quit.
 */

static int ps_pausepage_menucb_quit(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_input_request_termination()<0) return -1;
  return 0;
}
