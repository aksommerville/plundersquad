#include "ps.h"
#include "gui/ps_gui.h"
#include "input/ps_input.h"
#include "input/ps_input_device.h"
#include "game/ps_game.h"
#include "game/ps_sound_effects.h"

#define PS_HEROSELECT_MARGIN   5
#define PS_HEROSELECT_SPACING  5
#define PS_HEROSELECT_TRANSITION_TIME 20

static int ps_heroselect_layout_children(struct ps_widget *widget);

/* Widget definition.
 */

struct ps_widget_heroselect {
  struct ps_widget hdr;
  int watchid_devices;
};

#define WIDGET ((struct ps_widget_heroselect*)widget)

/* Device connection callbacks.
 */

static int ps_heroselect_cb_connect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_input_device_is_usable(device)) {
    if (ps_widget_heroselect_add_input_device(widget,device)<0) return -1;
  }
  return 0;
}

static int ps_heroselect_cb_disconnect(struct ps_input_device *device,void *userdata) {
  struct ps_widget *widget=userdata;
  if (ps_widget_heroselect_remove_input_device(widget,device)<0) return -1;
  return 0;
}

/* Delete.
 */

static void _ps_heroselect_del(struct ps_widget *widget) {
  ps_input_unwatch_devices(WIDGET->watchid_devices);
}

/* Initialize.
 */

static int _ps_heroselect_init(struct ps_widget *widget) {
  return 0;
}

/* Draw.
 */

static int _ps_heroselect_draw(struct ps_widget *widget,int x0,int y0) {
  if (ps_widget_draw_background(widget,x0,y0)<0) return -1;
  if (ps_widget_draw_children(widget,x0,y0)<0) return -1;
  return 0;
}

/* Measure.
 * Our desired dimensions are constant, based on the screen size.
 */

static int _ps_heroselect_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=(PS_SCREENW*7)/8;
  *h=(PS_SCREENH*2)/3;
  return 0;
}

/* Pack.
 */

static int _ps_heroselect_pack(struct ps_widget *widget) {
  if (ps_heroselect_layout_children(widget)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_heroselect={
  .name="heroselect",
  .objlen=sizeof(struct ps_widget_heroselect),
  .del=_ps_heroselect_del,
  .init=_ps_heroselect_init,
  .draw=_ps_heroselect_draw,
  .measure=_ps_heroselect_measure,
  .pack=_ps_heroselect_pack,
};

/* Enable input.
 */

int ps_widget_heroselect_enable_input(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  if (WIDGET->watchid_devices>0) ps_input_unwatch_devices(WIDGET->watchid_devices);
  if ((WIDGET->watchid_devices=ps_input_watch_devices(ps_heroselect_cb_connect,ps_heroselect_cb_disconnect,widget))<0) return -1;
  return 0;
}

/* Choose position for all children and animate their transition to it.
 * Call this after adding or removing a child.
 */

static int ps_heroselect_layout_children(struct ps_widget *widget) {
  if (widget->childc<1) return 0;

  struct ps_gui *gui=ps_widget_get_gui(widget);
  if (!gui) return 0; // Can happen during construction; take it on faith that we will be packed later.

  int rowc=(widget->childc+3)/4; // How many rows.
  int colc=widget->childc/rowc; // Minimum columns per row.
  int colextra=widget->childc%rowc; // Count of rows with one extra column.

  int outerx=PS_HEROSELECT_MARGIN;
  int outerw=widget->w-(PS_HEROSELECT_MARGIN<<1);
  int outery=PS_HEROSELECT_MARGIN;
  int outerh=widget->h-(PS_HEROSELECT_MARGIN<<1);

  int rowoffset=outerh/rowc;
  int coloffset=outerw/colc;
  int coloffsetextra=outerw/(colc+1);

  int row=0,col=0;
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    int childx,childy,childw,childh;

    childy=outery+row*rowoffset;
    childh=rowoffset;
    if (row<rowc-1) childh-=PS_HEROSELECT_SPACING;
    if (childh<1) childh=1;

    int coloff=(colextra?coloffsetextra:coloffset);
    childx=outerx+col*coloff;
    childw=coloff;
    if (col<colc-1+(colextra?1:0)) childw-=PS_HEROSELECT_SPACING;
    if (childw<1) childw=1;

    /* If we don't have a GUI master for some reason, set dimensions directly. */
    if (!gui) {
      child->x=childx;
      child->y=childy;
      child->w=childw;
      child->h=childh;

    /* If the child is already in use, animate from where it is. */
    } else if (child->w) {
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_x,childx,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_y,childy,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_w,childw,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_h,childh,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;

    /* Child is not initialized, set it infinitely small in the middle of its zone, then pop up. */
    } else {
      child->x=childx+(childw>>1);
      child->y=childy+(childh>>1);
      child->w=1;
      child->h=1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_x,childx,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_y,childy,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_w,childw,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
      if (ps_gui_transition_property(gui,child,PS_GUI_PROPERTY_h,childh,PS_HEROSELECT_TRANSITION_TIME)<0) return -1;
    }
      
    col++;
    if (colextra) {
      if (col>colc) { colextra--; col=0; row++; }
    } else {
      if (col>=colc) { col=0; row++; }
    }
  }
  
  return 0;
}

/* Find child by input device.
 */

static struct ps_widget *ps_heroselect_find_herosetup_for_device(const struct ps_widget *widget,const struct ps_input_device *device) {
  int i=widget->childc; while (i-->0) {
    struct ps_widget *herosetup=widget->childv[i];
    if (ps_widget_herosetup_get_input_device(herosetup)==device) return herosetup;
  }
  return 0;
}

/* Add device.
 */

int ps_widget_heroselect_add_input_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  if (!device) return -1;

  struct ps_widget *herosetup=ps_widget_spawn(widget,&ps_widget_type_herosetup);
  if (!herosetup) return -1;
  if (ps_widget_herosetup_set_input_device(herosetup,device)<0) return -1;
  if (ps_widget_herosetup_retreat_phase(herosetup)<0) return -1;

  if (ps_heroselect_layout_children(widget)<0) return -1;

  return 0;
}

/* Remove device.
 */
 
int ps_widget_heroselect_remove_input_device(struct ps_widget *widget,struct ps_input_device *device) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  if (!device) return -1;

  struct ps_widget *herosetup=ps_heroselect_find_herosetup_for_device(widget,device);
  if (!herosetup) return 0;

  if (ps_widget_remove_child(widget,herosetup)<0) return -1;

  if (ps_heroselect_layout_children(widget)<0) return -1;
  
  return 0;
}

/* Receive input from dependent herosetup widget.
 */
 
int ps_widget_heroselect_receive_player_input(struct ps_widget *widget,struct ps_widget *herosetup,int btnid,int value) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  if (!herosetup||(herosetup->type!=&ps_widget_type_herosetup)) return -1;

  // Not interested in key-up events.
  if (!value) return 0;

  switch (btnid) {
    case PS_PLRBTN_A: if (ps_widget_herosetup_advance_phase(herosetup)<0) return -1; break;
    case PS_PLRBTN_B: if (ps_widget_herosetup_retreat_phase(herosetup)<0) return -1; break;

    #define ADJUST(tag,d) if (ps_widget_herosetup_get_phase(herosetup)==PS_HEROSETUP_PHASE_EDIT) { \
        struct ps_game *game=ps_gui_get_game(ps_widget_get_gui(widget)); \
        if (game) { \
          PS_SFX_MENU_ADJUST \
          int playerid=ps_widget_herosetup_get_playerid(herosetup); \
          if (ps_game_adjust_player_##tag(ps_gui_get_game(ps_widget_get_gui(widget)),playerid,d)<0) return -1; \
          if (ps_widget_herosetup_refresh_player(herosetup,game->playerv[playerid-1])<0) return -1; \
        } \
      } break;
    case PS_PLRBTN_LEFT: ADJUST(definition,-1)
    case PS_PLRBTN_RIGHT: ADJUST(definition,1)
    case PS_PLRBTN_UP: ADJUST(palette,-1)
    case PS_PLRBTN_DOWN: ADJUST(palette,1)
    #undef ADJUST
  }

  return 0;
}

/* Get a new player id.
 */
 
int ps_widget_heroselect_allocate_player(struct ps_widget *widget) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);
  if (!game) return -1;
  if (game->playerc>=PS_PLAYER_LIMIT) return -1;
  if (ps_game_set_player_count(game,game->playerc+1)<0) return -1;
  if (ps_game_adjust_player_definition(game,game->playerc,1)<0) return -1;
  return game->playerc;
}

/* Relinquish a player id.
 */
 
int ps_widget_heroselect_free_player(struct ps_widget *widget,int playerid) {
  if (!widget||(widget->type!=&ps_widget_type_heroselect)) return -1;
  if ((playerid<1)||(playerid>PS_PLAYER_LIMIT)) return 0;
  struct ps_gui *gui=ps_widget_get_gui(widget);
  struct ps_game *game=ps_gui_get_game(gui);
  if (!game) return -1;

  if (ps_game_eliminate_player(game,playerid)<0) return -1;

  /* Adjust playerid of all herosetups above this one. */
  int i=widget->childc; while (i-->0) {
    struct ps_widget *herosetup=widget->childv[i];
    int playerid1=ps_widget_herosetup_get_playerid(herosetup);
    if (playerid1>playerid) {
      if (ps_widget_herosetup_set_playerid(herosetup,playerid1-1)<0) return -1;
    }
  }
  
  return 0;
}
