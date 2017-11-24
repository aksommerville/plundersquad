/* ps_gui.h
 * Container for interactive menus and such.
 * There are three major classes:
 *  - ps_gui is the global container.
 *  - ps_page is a single screen of content with controller logic.
 *  - ps_widget is the basic element of which pages are composed.
 */

#ifndef PS_GUI_H
#define PS_GUI_H

#include "ps_page.h"
#include "ps_widget.h"
#include "ps_widget_types.h"

struct ps_game;

/* Global GUI.
 * Only one ps_gui needs to be instantiated, but you're free to make as many as you like.
 */

struct ps_gui *ps_gui_new();
void ps_gui_del(struct ps_gui *gui);
int ps_gui_ref(struct ps_gui *gui);

int ps_gui_is_active(const struct ps_gui *gui);
int ps_gui_update(struct ps_gui *gui);

int ps_gui_load_page(struct ps_gui *gui,struct ps_page *page);
int ps_gui_unload_page(struct ps_gui *gui);

int ps_gui_set_game(struct ps_gui *gui,struct ps_game *game);
struct ps_game *ps_gui_get_game(const struct ps_gui *gui);

/* Conveniences to instantiate a new page and load it.
 */

int ps_gui_load_page_assemble(struct ps_gui *gui);
int ps_gui_load_page_sconfig(struct ps_gui *gui);
int ps_gui_load_page_pconfig(struct ps_gui *gui);
int ps_gui_load_page_pause(struct ps_gui *gui);
int ps_gui_load_page_debug(struct ps_gui *gui);
int ps_gui_load_page_gameover(struct ps_gui *gui);
int ps_gui_load_page_edithome(struct ps_gui *gui);
int ps_gui_load_page_editsfx(struct ps_gui *gui);
int ps_gui_load_page_editsong(struct ps_gui *gui);
int ps_gui_load_page_editblueprint(struct ps_gui *gui);
int ps_gui_load_page_editsprdef(struct ps_gui *gui);
int ps_gui_load_page_editplrdef(struct ps_gui *gui);

/* Input events.
 */

int ps_gui_move_cursor(struct ps_gui *gui,int dx,int dy);
int ps_gui_activate_cursor(struct ps_gui *gui);
int ps_gui_cancel_page(struct ps_gui *gui);
int ps_gui_submit_page(struct ps_gui *gui);

// Caller must translate (x,y) into framebuffer coordinates.
int ps_gui_event_mmotion(struct ps_gui *gui,int x,int y);
int ps_gui_event_mbutton(struct ps_gui *gui,int btnid,int value);
int ps_gui_event_mwheel(struct ps_gui *gui,int dx,int dy);

struct ps_widget *ps_gui_find_widget_at_point(const struct ps_gui *gui,int x,int y);

/* Animation.
 */

#define PS_GUI_PROPERTY_bgrgba    1
#define PS_GUI_PROPERTY_fgrgba    2
#define PS_GUI_PROPERTY_x         3
#define PS_GUI_PROPERTY_y         4
#define PS_GUI_PROPERTY_w         5
#define PS_GUI_PROPERTY_h         6

int ps_gui_transition_property(struct ps_gui *gui,struct ps_widget *widget,int k,int v,int duration);
int ps_gui_animate_property(struct ps_gui *gui,struct ps_widget *widget,int k,int va,int vz,int duration);
int ps_gui_finish_transitions(struct ps_gui *gui);

#endif
