#include "ps_gui_internal.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "input/ps_input.h"

#define PS_GUI_BLOTTER_COLOR 0x000000c0

/* Draw, master.
 */

static int _ps_gui_layer_draw(struct ps_video_layer *layer) {
  struct ps_gui *gui=((struct ps_gui_layer*)layer)->gui;

  if (gui->page) {
    if (gui->page->root) {
      if (ps_widget_draw(gui->page->root,0,0)<0) return -1;
    }
    if (gui->page->modalc) {
      if (ps_video_draw_rect(0,0,PS_SCREENW,PS_SCREENH,PS_GUI_BLOTTER_COLOR)<0) return -1;
      int i=0; for (;i<gui->page->modalc;i++) {
        struct ps_widget *modal=gui->page->modalv[i];
        if (ps_widget_draw(modal,0,0)<0) return -1;
      }
    }
  }

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

/* GUI object lifecycle.
 */

struct ps_gui *ps_gui_new() {

  struct ps_gui *gui=calloc(1,sizeof(struct ps_gui));
  if (!gui) return 0;

  gui->refc=1;
  gui->use_unified_input=1;

  return gui;
}

void ps_gui_del(struct ps_gui *gui) {
  if (!gui) return;
  if (gui->refc-->1) return;

  ps_widget_del(gui->track_hover);
  ps_widget_del(gui->track_click);

  if (gui->transitionv) {
    while (gui->transitionc-->0) {
      ps_transition_cleanup(gui->transitionv+gui->transitionc);
    }
    free(gui->transitionv);
  }

  ps_page_del(gui->page);
  ps_video_uninstall_layer(gui->layer);
  ps_video_layer_del(gui->layer);

  free(gui);
}

int ps_gui_ref(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->refc<1) return -1;
  if (gui->refc==INT_MAX) return -1;
  gui->refc++;
  return 0;
}

/* Trivial accessors.
 */

int ps_gui_set_game(struct ps_gui *gui,struct ps_game *game) {
  if (!gui) return -1;
  gui->game=game;
  return 0;
}

struct ps_game *ps_gui_get_game(const struct ps_gui *gui) {
  if (!gui) return 0;
  return gui->game;
}

/* Test active.
 */
 
int ps_gui_is_active(const struct ps_gui *gui) {
  if (!gui) return 0;
  if (!gui->page) return 0;
  return 1;
}

/* Update.
 */

int ps_gui_update(struct ps_gui *gui) {
  if (!gui) return -1;

  uint16_t ninput=ps_get_player_buttons(0);
  if (ninput!=gui->input) {
    if (gui->use_unified_input) {
      #define BTNDOWN(tag,action) \
        if ((ninput&PS_PLRBTN_##tag)&&!(gui->input&PS_PLRBTN_##tag)) { \
          if (action<0) return -1; \
        }
      BTNDOWN(UP,ps_gui_move_cursor(gui,0,-1))
      BTNDOWN(DOWN,ps_gui_move_cursor(gui,0,1))
      BTNDOWN(LEFT,ps_gui_move_cursor(gui,-1,0))
      BTNDOWN(RIGHT,ps_gui_move_cursor(gui,1,0))
      BTNDOWN(A,ps_gui_activate_cursor(gui))
      BTNDOWN(B,ps_gui_cancel_page(gui))
      BTNDOWN(START,ps_gui_submit_page(gui))
      #undef BTNDOWN
    }
    gui->input=ninput;
  }

  if (ps_gui_update_transitions(gui)<0) return -1;

  if (gui->page) {
    if (ps_page_update(gui->page)<0) return -1;
  }
  
  return 0;
}

/* Process input events.
 */
 
int ps_gui_move_cursor(struct ps_gui *gui,int dx,int dy) {
  if (!gui) return -1;
  if (gui->page) {
    if (ps_page_move_cursor(gui->page,dx,dy)<0) return -1;
  }
  return 0;
}

int ps_gui_activate_cursor(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (ps_page_activate(gui->page)<0) return -1;
  }
  return 0;
}

int ps_gui_cancel_page(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (ps_page_cancel(gui->page)<0) return -1;
  }
  return 0;
}

int ps_gui_submit_page(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (ps_page_submit(gui->page)<0) return -1;
  }
  return 0;
}

/* Mouse motion.
 */

int ps_gui_event_mmotion(struct ps_gui *gui,int x,int y) {
  if (!gui) return -1;
  if ((x==gui->mousex)&&(y==gui->mousey)) return 0;
  gui->mousex=x;
  gui->mousey=y;

  /* If we are hovering over something, deliver the motion event. */
  if (gui->track_hover) {
    int subx,suby;
    if (ps_widget_coords_from_window(&subx,&suby,gui->track_hover,x,y)<0) return -1;
    if (ps_widget_event_mousemove(gui->track_hover,subx,suby)<0) return -1;
  }

  /* If we are tracking a click, only the subject of the click can receive "hover" focus. */
  if (gui->track_click) {
    int inbounds=ps_widget_contains_point(gui->track_click,x,y);
    if (inbounds) {
      if (gui->track_hover==gui->track_click) return 0;
      if (ps_widget_ref(gui->track_click)<0) return -1;
      ps_widget_del(gui->track_hover);
      gui->track_hover=gui->track_click;
      if (ps_widget_event_mouseenter(gui->track_hover)<0) return -1;
    } else {
      if (!gui->track_hover) return 0;
      if (ps_widget_event_mouseexit(gui->track_hover)<0) return -1;
      ps_widget_del(gui->track_hover);
      gui->track_hover=0;
    }
    return 0;
  }

  /* No click tracking in progress, so focus anything. */
  struct ps_widget *nhover=ps_gui_find_widget_at_point(gui,x,y);
  if (nhover==gui->track_hover) return 0;

  if (gui->track_hover) {
    if (ps_widget_event_mouseexit(gui->track_hover)<0) return -1;
  }
  
  if (nhover&&(ps_widget_ref(nhover)<0)) return -1;
  ps_widget_del(gui->track_hover);
  gui->track_hover=nhover;

  if (nhover) {
    if (ps_widget_event_mouseenter(nhover)<0) return -1;
  }
  
  return 0;
}

/* Mouse button.
 */

int ps_gui_event_mbutton(struct ps_gui *gui,int btnid,int value) {
  if (!gui) return -1;

  if (value) {
    if (gui->track_click) return 0; // Already tracking something, discard new events.
    if (!gui->track_hover) return 0; // Not hovering on anything.
    if (ps_widget_ref(gui->track_hover)<0) return -1;
    gui->track_click=gui->track_hover;
    gui->track_btnid=btnid;
    if (ps_widget_event_mousedown(gui->track_click,btnid)<0) return -1;

  } else {
    if (btnid!=gui->track_btnid) return 0;
    gui->track_btnid=0;
    if (gui->track_click) {
      if (ps_widget_event_mouseup(gui->track_click,btnid,(gui->track_hover==gui->track_click)?1:0)<0) return -1;
      ps_widget_del(gui->track_click);
      gui->track_click=0;
    }
  }
  return 0;
}

/* Mouse wheel.
 */

int ps_gui_event_mwheel(struct ps_gui *gui,int dx,int dy) {
  if (!gui) return -1;
  if (!dx&&!dy) return -1;
  if (gui->track_hover) {
    if (ps_widget_event_mousewheel(gui->track_hover,dx,dy)<0) return -1;
  }
  return 0;
}

/* Find widget at point.
 */

static struct ps_widget *ps_gui_find_widget_at_point_1(struct ps_widget *widget,int x,int y) {

  /* Doesn't intersect this widget? Return NULL. */
  if (x<widget->x) return 0;
  if (x>=widget->x+widget->w) return 0;
  if (y<widget->y) return 0;
  if (y>=widget->y+widget->h) return 0;

  /* Check my children, in reverse rendering order. Return the first hit. */
  x-=widget->x;
  y-=widget->y;
  int i=widget->childc; while (i-->0) {
    struct ps_widget *child=widget->childv[i];
    struct ps_widget *found=ps_gui_find_widget_at_point_1(child,x,y);
    if (found) return found;
  }

  /* Does this widget track mouse events? */
  if (!widget->track_mouse) return 0;
  return widget;
}
 
struct ps_widget *ps_gui_find_widget_at_point(const struct ps_gui *gui,int x,int y) {
  if (!gui) return 0;
  if (!gui->page) return 0;
  if (gui->page->modalc) {
    return ps_gui_find_widget_at_point_1(gui->page->modalv[gui->page->modalc-1],x,y);
  } else {
    return ps_gui_find_widget_at_point_1(gui->page->root,x,y);
  }
}

/* Load page.
 */

int ps_gui_load_page(struct ps_gui *gui,struct ps_page *page) {
  if (!gui||!page) return -1;
  if (!page->root) return -1;
  if (gui->page==page) return 0; // or should we restart it, or something?

  //TODO transitions and events on page load

  /* Unload the existing page first. */
  if (gui->page) {
    gui->page->gui=0;
    ps_page_del(gui->page);
    gui->page=0;
  }

  /* Retain the incoming page. */
  if (ps_page_ref(page)<0) return -1;
  gui->page=page;
  page->gui=gui;

  /* Pack the page's root.
   * This is nice and easy because the global framebuffer has a constant size known at compile time.
   */
  page->root->x=0;
  page->root->y=0;
  page->root->w=PS_SCREENW;
  page->root->h=PS_SCREENH;
  if (ps_widget_pack(page->root)<0) return -1;

  if (ps_gui_init_layer(gui)<0) return -1;

  return 0;
}

/* Unload page.
 */
 
int ps_gui_unload_page(struct ps_gui *gui) {
  if (!gui) return -1;
  if (!gui->page) return 0;

  gui->page->gui=0;
  ps_page_del(gui->page);
  gui->page=0;

  if (ps_gui_drop_layer(gui)<0) return -1;

  return 0;
}

/* Conveniences to instantiate and load a page.
 */

#define LOADER(tag) \
  int ps_gui_load_page_##tag(struct ps_gui *gui) { \
    struct ps_page *page=ps_page_new(&ps_page_type_##tag); \
    if (!page) return -1; \
    if (ps_gui_load_page(gui,page)<0) { \
      ps_page_del(page); \
      return -1; \
    } \
    ps_page_del(page); \
    return 0; \
  }
#define STUBLOADER(tag) \
  int ps_gui_load_page_##tag(struct ps_gui *gui) { \
    ps_log(GUI,ERROR,"Page type '"#tag"' stubbed."); \
    return -1; \
  }

LOADER(assemble)
LOADER(sconfig)
STUBLOADER(pconfig)
LOADER(pause)
STUBLOADER(debug)
LOADER(gameover)
LOADER(edithome)
LOADER(editsfx)
LOADER(editsong)
STUBLOADER(editblueprint)
STUBLOADER(editsprdef)
STUBLOADER(editplrdef)

#undef LOADER
#undef STUBLOADER
