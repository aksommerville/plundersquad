#include "ps_gui_internal.h"
#include "game/ps_game.h"
#include "corewidgets/ps_corewidgets.h"
#include "menus/ps_menus.h"
#include "editor/ps_editor.h"
#include "video/ps_video.h"
#include "akau/akau.h"

/* Global.
 */

struct ps_gui *ps_gui_global=0;

struct ps_gui *ps_get_global_gui() {
  return ps_gui_global;
}

void ps_drop_global_gui() {
  ps_gui_del(ps_gui_global);
  ps_gui_global=0;
}

int ps_gui_is_global(const struct ps_gui *gui) {
  if (!gui) return 0;
  return (gui==ps_gui_global)?1:0;
}

/* Draw, master.
 */

static int _ps_gui_layer_draw(struct ps_video_layer *layer) {
  struct ps_gui *gui=((struct ps_gui_layer*)layer)->gui;
  if (ps_widget_draw(gui->root,0,0)<0) return -1;
  return 0;
}

/* Initialize video layer.
 */

static int ps_gui_init_layer(struct ps_gui *gui) {

  if (gui->layer) return 0;

  gui->layer=ps_video_layer_new(sizeof(struct ps_gui_layer));
  if (!gui->layer) return -1;

  gui->layer->blackout=0;
  gui->layer->draw=_ps_gui_layer_draw;
  LAYER->gui=gui;

  if (ps_video_install_layer(gui->layer,-1)<0) return -1;

  return 0;
}

static int ps_gui_drop_layer(struct ps_gui *gui) {

  if (!gui->layer) return 0;

  LAYER->gui=0;
  ps_video_uninstall_layer(gui->layer);
  ps_video_layer_del(gui->layer);
  gui->layer=0;

  return 0;
}

/* New.
 */

struct ps_gui *ps_gui_new() {
  struct ps_gui *gui=calloc(1,sizeof(struct ps_gui));
  if (!gui) return 0;

  gui->refc=1;

  if (!ps_gui_global) {
    if (ps_gui_ref(gui)<0) {
      ps_gui_del(gui);
      return 0;
    }
    ps_gui_global=gui;
  }

  if (!(gui->root=ps_widget_new(&ps_widget_type_root))) {
    ps_gui_del(gui);
    return 0;
  }
  if (ps_widget_root_set_gui(gui->root,gui)<0) {
    ps_gui_del(gui);
    return 0;
  }
  gui->root->w=PS_SCREENW;
  gui->root->h=PS_SCREENH;

  if (ps_gui_init_layer(gui)<0) {
    ps_gui_del(gui);
    return 0;
  }

  return gui;
}

/* Delete.
 */

void ps_gui_del(struct ps_gui *gui) {
  if (!gui) return;
  if (gui->refc-->1) return;

  if (gui==ps_gui_global) ps_gui_global=0;
  
  ps_video_uninstall_layer(gui->layer);
  ps_video_layer_del(gui->layer);

  if (gui->animationv) {
    while (gui->animationc-->0) ps_animation_del(gui->animationv[gui->animationc]);
    free(gui->animationv);
  }

  ps_widget_del(gui->root);
  ps_userconfig_del(gui->userconfig);

  free(gui);
}

/* Retain.
 */

int ps_gui_ref(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->refc<1) return -1;
  if (gui->refc==INT_MAX) return -1;
  gui->refc++;
  return 0;
}

/* Test active.
 */

int ps_gui_is_active(const struct ps_gui *gui) {
  if (!gui) return 0;
  if (!gui->root) return 0;
  return gui->root->childc?1:0;
}

/* External linkage.
 */
 
struct ps_game *ps_gui_get_game(const struct ps_gui *gui) {
  if (!gui) return 0;
  return gui->game;
}

int ps_gui_set_game(struct ps_gui *gui,struct ps_game *game) {
  if (!gui) return -1;
  if (gui->game==game) return 0;
  gui->game=game;
  return 0;
}

struct ps_userconfig *ps_gui_get_userconfig(const struct ps_gui *gui) {
  if (!gui) return 0;
  return gui->userconfig;
}

int ps_gui_set_userconfig(struct ps_gui *gui,struct ps_userconfig *userconfig) {
  if (!gui) return -1;
  if (gui->userconfig==userconfig) return 0;
  if (userconfig) {
    if (ps_userconfig_ref(userconfig)<0) return -1;
    ps_userconfig_del(gui->userconfig);
    gui->userconfig=userconfig;
  } else {
    ps_userconfig_del(gui->userconfig);
    gui->userconfig=0;
  }
  return 0;
}

/* Trivial accessors.
 */

struct ps_widget *ps_gui_get_root(const struct ps_gui *gui) {
  if (!gui) return 0;
  return gui->root;
}

/* Update.
 */
 
int ps_gui_update(struct ps_gui *gui) {
  if (!gui) return -1;
  
  if (ps_widget_update(gui->root)<0) return -1;

  if (ps_animations_update(gui->animationv,gui->animationc)<0) return -1;
  gui->animationc=ps_animations_gc(gui->animationv,gui->animationc);
  
  return 0;
}

/* Deliver general input events.
 */

int ps_gui_mousemotion(struct ps_gui *gui,int x,int y) {
  if (!gui) return -1;
  if (ps_widget_mousemotion(gui->root,x,y)<0) return -1;
  return 0;
}

int ps_gui_mousebutton(struct ps_gui *gui,int btnid,int value) {
  if (!gui) return -1;
  if (ps_widget_mousebutton(gui->root,btnid,value)<0) return -1;
  return 0;
}

int ps_gui_mousewheel(struct ps_gui *gui,int dx,int dy) {
  if (!gui) return -1;
  if (ps_widget_mousewheel(gui->root,dx,dy)<0) return -1;
  return 0;
}

int ps_gui_key(struct ps_gui *gui,int keycode,int codepoint,int value) {
  if (!gui) return -1;
  if (ps_widget_key(gui->root,keycode,codepoint,value)<0) return -1;
  return 0;
}

int ps_gui_userinput(struct ps_gui *gui,int plrid,int btnid,int value) {
  if (!gui) return -1;
  if (ps_widget_userinput(gui->root,plrid,btnid,value)<0) return -1;
  return 0;
}

/* Animate.
 */

int ps_gui_animate_property(
  struct ps_gui *gui,
  struct ps_widget *widget,
  int k,
  int v,
  int duration
) {
  if (!gui||!widget) return -1;

  if (gui->animationc>=gui->animationa) {
    int na=gui->animationa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(gui->animationv,sizeof(void*)*na);
    if (!nv) return -1;
    gui->animationv=nv;
    gui->animationa=na;
  }

  struct ps_animation *animation=ps_animation_new(widget);
  if (!animation) return -1;
  if (ps_animation_setup(animation,k,v,duration)<0) {
    ps_animation_del(animation);
    return -1;
  }

  gui->animationv[gui->animationc++]=animation;

  return 0;
}

/* Load main pages.
 */
 
int ps_gui_load_page_assemble(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_assemblepage);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  akau_play_song(5,1);
  return 0;
}
 
int ps_gui_load_page_setup(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_setuppage);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}

int ps_gui_load_page_pause(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_pausepage);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}

int ps_gui_load_page_debugmenu(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_debugmenu);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}

int ps_gui_load_page_gameover(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_gameoverpage);
  if (!page) return -1;
  if (ps_widget_gameoverpage_setup(page,gui->game)<0) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  akau_play_song(3,1);
  return 0;
}

int ps_gui_load_page_inputcfg(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_inputcfgpage);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}

int ps_gui_load_page_treasure(struct ps_gui *gui,const struct ps_res_trdef *trdef) {
  if (!gui||!trdef) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_treasurealert);
  if (!page) return -1;
  if (ps_widget_treasurealert_setup(page,trdef)<0) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}

int ps_gui_load_page_audiocfg(struct ps_gui *gui) {
  if (!gui) return -1;
  struct ps_widget *page=ps_widget_spawn(gui->root,&ps_widget_type_audiocfgpage);
  if (!page) return -1;
  if (ps_widget_pack(gui->root)<0) return -1;
  return 0;
}
