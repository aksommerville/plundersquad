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
#include "game/ps_player.h"
#include "game/ps_plrdef.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "video/ps_video.h"
#include "res/ps_resmgr.h"

#define PS_PAUSEPAGE_CHEAT_LIMIT 8
#define PS_PAUSEPAGE_CHEAT_TIME  2000000
#define PS_PAUSEPAGE_DEFAULT_THUMB_COLOR 0x206080ff
#define PS_PAUSEPAGE_TEXT_COLOR 0xffffffff

static int ps_pausepage_menucb_resume(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_to_beginning(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_restart(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_end_game(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_input(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_audio(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_fullscreen(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_menucb_quit(struct ps_widget *button,struct ps_widget *widget);
static int ps_pausepage_identify_controlling_player(struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_pausepage {
  struct ps_widget hdr;
  int cheat_btnidv[PS_PAUSEPAGE_CHEAT_LIMIT];
  int cheat_btnidp;
  int64_t cheat_expiration;
  int playerid; // Which player is controlling us? If zero, accept all input.
  int init; // Defer some initialization until first update.
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
  thumb->bgrgba=PS_PAUSEPAGE_DEFAULT_THUMB_COLOR;

  #define MENUITEM(label,cbtag) { \
    struct ps_widget *item=ps_widget_menu_spawn_button(menu,label,sizeof(label)-1,ps_callback(ps_pausepage_menucb_##cbtag,0,widget)); \
    if (!item) return -1; \
    if (ps_widget_button_set_text_color(item,PS_PAUSEPAGE_TEXT_COLOR)<0) return -1; \
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

  ps_pausepage_identify_controlling_player(widget);
  
  return 0;
}

/* Pack.
 */

static int _ps_pausepage_pack(struct ps_widget *widget) {
  if (widget->childc<1) return -1;
  
  struct ps_widget *menu=widget->childv[0];
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,menu,widget->w,widget->h)<0) return -1;
  menu->x=(widget->w>>1)-(chw>>1);
  menu->y=(widget->h>>1)-(chh>>1);
  menu->w=chw;
  menu->h=chh;
  if (ps_widget_pack(menu)<0) return -1;

  if (widget->childc>=2) {
    struct ps_widget *sprite=widget->childv[1];
    if (ps_widget_measure(&chw,&chh,sprite,widget->w,widget->h)<0) return -1;
    sprite->x=(widget->w>>1)-(chw>>1);
    sprite->y=menu->y-chh;
    sprite->w=chw;
    sprite->h=chh;
    if (ps_widget_pack(sprite)<0) return -1;
  }
  
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
  if (widget->childc<1) return -1;
  if (plrid&&WIDGET->playerid&&(plrid!=WIDGET->playerid)) {
    //ps_log(GUI,DEBUG,"Discarding event %d.%d=%d due to WIDGET->playerid==%d",plrid,btnid,value,WIDGET->playerid);
    return 0;
  }
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

/* Which player should be in charge?
 * Set WIDGET->playerid if only one player is holding START right now.
 * Since this is fallible, we can always default to zero and allow all input.
 */

static int ps_pausepage_identify_controlling_player(struct ps_widget *widget) {
  WIDGET->playerid=0;
  int playerid=PS_PLAYER_LIMIT; while (playerid-->1) {
    uint16_t buttons=ps_get_player_buttons(playerid);
    if (buttons&PS_PLRBTN_START) {
      if (WIDGET->playerid) {
        ps_log(GUI,DEBUG,"Multiple players pressing START -- anyone can control pausepage.");
        WIDGET->playerid=0;
        return 0;
      } else {
        WIDGET->playerid=playerid;
      }
    }
  }
  if (WIDGET->playerid) {
    ps_log(GUI,DEBUG,"Player %d controlling pausepage exclusively.",WIDGET->playerid);
  } else {
    // There is at least one update cycle between triggering pause (in ps_game) and creating the widget.
    // So this is theoretically possible, and I can reproduce it without much trouble on my Mac keyboard.
    // I don't think it's too big a deal.
    ps_log(GUI,DEBUG,"No player holding START -- anyone can control pausepage.");
  }
  return 0;
}

/* Select a thumb color with adequate contrast against both background and text.
 */

static uint32_t ps_pausepage_ensure_thumb_contrast(uint32_t fg,uint32_t bg,uint32_t text) {
  uint8_t *fgv=(uint8_t*)&fg;
  uint8_t *bgv=(uint8_t*)&bg;
  uint8_t *textv=(uint8_t*)&text;

  /* Assess contrast and keep the default if within tolerance. */
  int contrast_bg=0,contrast_text=0,i;
  for (i=0;i<4;i++) {
    if (fgv[i]>bgv[i]) contrast_bg+=fgv[i]-bgv[i];
    else contrast_bg+=bgv[i]-fgv[i];
    if (fgv[i]>textv[i]) contrast_text+=fgv[i]-textv[i];
    else contrast_text+=textv[i]-fgv[i];
  }
  if ((contrast_bg>=256)&&(contrast_text>=256)) return fg; //TODO tweak threshold

  /* For each channel, go to a limit if bg and text are on the same side of 0x80, otherwise split them.
   */
  for (i=0;i<4;i++) {
    if ((bgv[i]&0x80)&&(textv[i]&0x80)) {
      fgv[i]=0x00;
    } else if ((bgv[i]&0x80)||(textv[i]&0x80)) {
      fgv[i]=(bgv[i]+textv[i])>>1;
    } else {
      fgv[i]=0xff;
    }
  }
  fg|=0x000000ff;
  return fg;
}

static uint32_t ps_rgba_darken(uint32_t src) {
  uint8_t *v=(uint8_t*)&src;
  int i; for (i=0;i<4;i++) v[i]>>=1;
  return src;
}

/* If we have an exclusive controlling player, update my appearance to reflect it.
 */
 
int ps_widget_pausepage_skin_for_player(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_pausepage)) return 0;

  /* Drop sprite widget if present. */
  while (widget->childc>1) {
    ps_widget_remove_child(widget,widget->childv[widget->childc-1]);
  }

  /* Find the controlling player, or terminate. */  
  if ((WIDGET->playerid<1)||(WIDGET->playerid>PS_PLAYER_LIMIT)) return 0;
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (!game) return 0;
  const struct ps_player *player=game->playerv[WIDGET->playerid-1];
  if (!player) return 0;
  ps_log(GUI,DEBUG,"Skinning pausepage for player %d.",WIDGET->playerid);

  /* Set my background color based on hero's body color. */
  if (player->plrdef&&(player->palette<player->plrdef->palettec)) {
    widget->bgrgba=(ps_rgba_darken(player->plrdef->palettev[player->palette].rgba_body)&0xffffff00)|0x000000c0;
    struct ps_widget *thumb=ps_widget_menu_get_thumb(widget->childv[0]);
    if (thumb) {
      thumb->bgrgba=ps_pausepage_ensure_thumb_contrast(PS_PAUSEPAGE_DEFAULT_THUMB_COLOR,widget->bgrgba,PS_PAUSEPAGE_TEXT_COLOR);
    }
  }

  /* Create a widget for the hero sprite. */
  struct ps_widget *sprite=ps_widget_spawn(widget,&ps_widget_type_sprite);
  if (!sprite) return -1;
  if (ps_widget_sprite_load_sprdef(sprite,1)<0) return -1;
  if (ps_widget_sprite_set_plrdefid(sprite,ps_res_get_id_by_obj(PS_RESTYPE_PLRDEF,player->plrdef))<0) return -1;
  if (ps_widget_sprite_set_palette(sprite,player->palette)<0) return -1;
  if (ps_widget_sprite_set_action_walk_down(sprite)<0) return -1;

  return 0;
}
