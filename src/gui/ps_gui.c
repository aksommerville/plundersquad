#include "ps_gui_internal.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "input/ps_input.h"

/* Draw, master.
 */

static int _ps_gui_layer_draw(struct ps_video_layer *layer) {
  struct ps_gui *gui=((struct ps_gui_layer*)layer)->gui;

  if (gui->page) {
    if (gui->page->root) {
      if (ps_widget_draw(gui->page->root,0,0)<0) return -1;
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

  gui->use_unified_input=1;

  return gui;
}

void ps_gui_del(struct ps_gui *gui) {
  if (!gui) return;

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
    if (gui->page->type->move_cursor) {
      if (gui->page->type->move_cursor(gui->page,dx,dy)<0) return -1;
    }
  }
  return 0;
}

int ps_gui_activate_cursor(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (gui->page->type->activate) {
      if (gui->page->type->activate(gui->page)<0) return -1;
    } else if (gui->page->type->submit) {
      if (gui->page->type->submit(gui->page)<0) return -1;
    }
  }
  return 0;
}

int ps_gui_cancel_page(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (gui->page->type->cancel) {
      if (gui->page->type->cancel(gui->page)<0) return -1;
    }
  }
  return 0;
}

int ps_gui_submit_page(struct ps_gui *gui) {
  if (!gui) return -1;
  if (gui->page) {
    if (gui->page->type->submit) {
      if (gui->page->type->submit(gui->page)<0) return -1;
    } else if (gui->page->type->activate) {
      if (gui->page->type->activate(gui->page)<0) return -1;
    }
  }
  return 0;
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
#define STUBLOADER(tag) int ps_gui_load_page_##tag(struct ps_gui *gui) { return -1; }

LOADER(assemble)
LOADER(sconfig)
STUBLOADER(pconfig)
LOADER(pause)
STUBLOADER(debug)
LOADER(gameover)

#undef LOADER
#undef STUBLOADER
