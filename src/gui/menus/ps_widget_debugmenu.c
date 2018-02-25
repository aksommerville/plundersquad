/* ps_widget_debugmenu.c
 * Special menu accessible from the pause menu with features only available while testing.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/ps_gui.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "game/ps_game.h"
#include "game/ps_switchboard.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "akpng/akpng.h"
#include <time.h>

static int ps_debugmenu_cb_menu(struct ps_widget *menu,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_debugmenu {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_debugmenu*)widget)

/* Delete.
 */

static void _ps_debugmenu_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_debugmenu_init(struct ps_widget *widget) {

  widget->bgrgba=0xffc080c0;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (!menupacker) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_debugmenu_cb_menu,0,widget))<0) return -1;

  struct ps_widget *thumb=ps_widget_menu_get_thumb(menu);
  if (!thumb) return -1;
  thumb->bgrgba=0x206080ff;

  struct ps_widget *label;
  if (!(label=ps_widget_menu_spawn_label(menu,"Back",-1))) return -1;
  if (!(label=ps_widget_menu_spawn_label(menu,"Toggle switches",-1))) return -1;
  if (!(label=ps_widget_menu_spawn_label(menu,"Swap input",-1))) return -1;
  if (!(label=ps_widget_menu_spawn_label(menu,"Heal all",-1))) return -1;
  if (!(label=ps_widget_menu_spawn_label(menu,"Screenshot",-1))) return -1;
  
  return 0;
}

/* Pack.
 */

static int _ps_debugmenu_pack(struct ps_widget *widget) {
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

/* Input.
 */

static int _ps_debugmenu_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (widget->childc<1) return -1;
  return ps_widget_userinput(widget->childv[0],plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_debugmenu={

  .name="debugmenu",
  .objlen=sizeof(struct ps_widget_debugmenu),

  .del=_ps_debugmenu_del,
  .init=_ps_debugmenu_init,

  .pack=_ps_debugmenu_pack,

  .userinput=_ps_debugmenu_userinput,

};

/* Dismiss menu.
 */

static int ps_debugmenu_back(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Heal all
 */

static int ps_debugmenu_heal_all(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_game_heal_all_heroes(game)<0) return -1;
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Swap input
 */

static int ps_debugmenu_swap_input(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  if (ps_input_swap_assignments()<0) return -1;
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Toggle all switches
 */

static int ps_debugmenu_toggle_switches(struct ps_widget *widget) {
  struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget));
  struct ps_switchboard *switchboard=game->switchboard;

  int swc=ps_switchboard_count_switches(switchboard);
  ps_log(GAME,INFO,"Toggling %d switch%s.",swc,(swc==1)?"":"s");
  int i=0; for (;i<swc;i++) {
    int swid;
    int value=ps_switchboard_get_switch_by_index(&swid,switchboard,i);
    if (ps_game_set_switch(game,swid,value?0:1)<0) return -1;
  }

  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(game,0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Compose path for screenshot.
 */

static int ps_debugmenu_compose_screenshot_path(char *dst,int dsta) {
  time_t now;
  struct tm tm={0};
  time(&now);
  if (localtime_r(&now,&tm)==&tm) {
    int year=tm.tm_year+1900;
    int month=tm.tm_mon+1;
    int day=tm.tm_mday;
    int hour=tm.tm_hour;
    int minute=tm.tm_min;
    int second=tm.tm_sec;
    return snprintf(dst,dsta,"plundersquad-%04d%02d%02d-%02d%02d%02d.png",year,month,day,hour,minute,second);
  } else {
    return snprintf(dst,dsta,"plundersquad-%d.png",(int)now);
  }
}

/* Screenshot.
 */

static int ps_debugmenu_screenshot(struct ps_widget *widget) {

  char path[1024];
  int pathc=ps_debugmenu_compose_screenshot_path(path,sizeof(path));
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;

  void *pixels=0;
  int pixelsc;
  if ((pixelsc=ps_video_capture_framebuffer_raw(&pixels,1))<0) {
    ps_log(GUI,ERROR,"Failed to capture pixels (ps_video_capture_framebuffer_raw())");
    return 0;
  }
  if (pixelsc!=PS_SCREENW*PS_SCREENH*4) {
    free(pixels);
    ps_log(GUI,ERROR,
      "ps_video_capture_framebuffer_raw() returned %d, expected %d (%d*%d*4)",
      pixelsc,PS_SCREENW*PS_SCREENH*4,PS_SCREENW,PS_SCREENH
    );
    return 0;
  }

  struct akpng_image png={
    .pixels=pixels, // HANDOFF
    .w=PS_SCREENW,
    .h=PS_SCREENH,
    .depth=8,
    .colortype=6,
    .stride=PS_SCREENW<<2,
  };
  if (akpng_encode_file(path,&png)<0) {
    akpng_image_cleanup(&png);
    ps_log(GUI,ERROR,"Failed to encode PNG to file '%s'.",path);
    return 0;
  }

  akpng_image_cleanup(&png);
  ps_log(GUI,INFO,"%s: Wrote PNG screenshot.",path);
  
  if (ps_input_suppress_player_actions(30)<0) return -1;
  if (ps_game_pause(ps_gui_get_game(ps_widget_get_gui(widget)),0)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_debugmenu_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  switch (ps_widget_menu_get_selected_index(menu)) {
    case 0: return ps_debugmenu_back(widget);
    case 1: return ps_debugmenu_toggle_switches(widget);
    case 2: return ps_debugmenu_swap_input(widget);
    case 3: return ps_debugmenu_heal_all(widget);
    case 4: return ps_debugmenu_screenshot(widget);
  }
  return 0;
}
