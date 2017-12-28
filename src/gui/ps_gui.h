/* ps_gui.h
 * The ps_gui object itself is really just a container for the root widget.
 */

#ifndef PS_GUI_H
#define PS_GUI_H

#include "util/ps_callback.h"

struct ps_gui;
struct ps_widget;
struct ps_game;

/* There should be only one ps_gui, and it should be created by the main unit.
 * The first ps_gui instantiated automatically becomes the global.
 * It is legal to create others, but they won't interact with the rest of the world.
 */
struct ps_gui *ps_get_global_gui();
void ps_drop_global_gui();

/* GUI object lifecycle.
 */
struct ps_gui *ps_gui_new();
void ps_gui_del(struct ps_gui *gui);
int ps_gui_ref(struct ps_gui *gui);

/* A GUI is "active" if it has widgets present to interact with the user.
 * We assume that there can also be a game present.
 * When the GUI is not active, the game is in use.
 * GUI should generally be "global". If not, it is a zombie.
 */
int ps_gui_is_active(const struct ps_gui *gui);
int ps_gui_is_global(const struct ps_gui *gui);

/* External linkage.
 */
struct ps_game *ps_gui_get_game(const struct ps_gui *gui);
int ps_gui_set_game(struct ps_gui *gui,struct ps_game *game);

/* Every GUI has a root widget which never changes.
 * The children of that widget swap out frequently.
 */
struct ps_widget *ps_gui_get_root(const struct ps_gui *gui);

/* Call every frame while active.
 */
int ps_gui_update(struct ps_gui *gui);

/* Deliver general input events.
 */
int ps_gui_mousemotion(struct ps_gui *gui,int x,int y);
int ps_gui_mousebutton(struct ps_gui *gui,int btnid,int value);
int ps_gui_mousewheel(struct ps_gui *gui,int dx,int dy);
int ps_gui_key(struct ps_gui *gui,int keycode,int codepoint,int value);
int ps_gui_userinput(struct ps_gui *gui,int plrid,int btnid,int value);

/* Schedule a widget's value to change over time.
 * (duration) is in frames, or more precisely, in calls to ps_gui_update().
 */
int ps_gui_animate_property(
  struct ps_gui *gui,
  struct ps_widget *widget,
  int k,
  int v,
  int duration
);

/* Conveniences to create a modal widget and push it on to the root.
 */
int ps_gui_load_page_assemble(struct ps_gui *gui);
int ps_gui_load_page_setup(struct ps_gui *gui);
int ps_gui_load_page_pause(struct ps_gui *gui);
int ps_gui_load_page_gameover(struct ps_gui *gui);
int ps_gui_load_page_edithome(struct ps_gui *gui);

#endif
