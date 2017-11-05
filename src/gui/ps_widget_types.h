/* ps_widget_types.h
 * Public header for specific widget types.
 */

#ifndef PS_WIDGET_TYPES_H
#define PS_WIDGET_TYPES_H

struct ps_input_device;
struct ps_player;

extern const struct ps_widget_type ps_widget_type_root; // Only for pages' root widgets. One full-size child.
extern const struct ps_widget_type ps_widget_type_label; // Single row of text.
extern const struct ps_widget_type ps_widget_type_packer; // Single column or row of widgets.
extern const struct ps_widget_type ps_widget_type_menu; // Vertical packer of labels with selection.
extern const struct ps_widget_type ps_widget_type_heroselect; // Container and controller for multiple herosetup widgets.
extern const struct ps_widget_type ps_widget_type_herosetup; // Panel for one input device, to click in and set up.
extern const struct ps_widget_type ps_widget_type_slider; // Label and horiztonal indicator, for use in menus.

/* Root.
 *****************************************************************************/

int ps_widget_root_set_page(struct ps_widget *widget,struct ps_page *page);
struct ps_page *ps_widget_root_get_page(const struct ps_widget *widget);

/* Label.
 *****************************************************************************/

int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc);

// Convenience to initialize a label and add to parent. Returns WEAK reference on success.
struct ps_widget *ps_widget_spawn_label(struct ps_widget *parent,const char *src,int srcc,uint32_t rgba);

/* Packer.
 *****************************************************************************/

int ps_widget_packer_setup(
  struct ps_widget *widget,
  int axis,
  int align_major,
  int align_minor,
  int border,
  int spacing
);

/* Menu.
 *****************************************************************************/

struct ps_widget *ps_widget_menu_add_option(struct ps_widget *widget,const char *text,int textc);
struct ps_widget *ps_widget_menu_add_slider(struct ps_widget *widget,const char *label,int labelc,int lo,int hi);

int ps_widget_menu_move_cursor(struct ps_widget *widget,int d);
int ps_widget_menu_adjust_selection(struct ps_widget *widget,int d);
int ps_widget_menu_get_cursor(const struct ps_widget *widget);
int ps_widget_menu_get_slider(const struct ps_widget *widget,int p);

/* Heroselect.
 *****************************************************************************/

int ps_widget_heroselect_enable_input(struct ps_widget *widget);
int ps_widget_heroselect_add_input_device(struct ps_widget *widget,struct ps_input_device *device);
int ps_widget_heroselect_remove_input_device(struct ps_widget *widget,struct ps_input_device *device);

// Dependant herosetups call this on device input.
int ps_widget_heroselect_receive_player_input(struct ps_widget *widget,struct ps_widget *herosetup,int btnid,int value);
int ps_widget_heroselect_allocate_player(struct ps_widget *widget);
int ps_widget_heroselect_free_player(struct ps_widget *widget,int playerid);

/* Herosetup.
 *****************************************************************************/

#define PS_HEROSETUP_PHASE_INIT      0 /* Click to join */
#define PS_HEROSETUP_PHASE_EDIT      1 /* Selecting hero */
#define PS_HEROSETUP_PHASE_READY     2 /* Waiting for teammates */
 
int ps_widget_herosetup_set_input_device(struct ps_widget *widget,struct ps_input_device *device);
struct ps_input_device *ps_widget_herosetup_get_input_device(const struct ps_widget *widget);

int ps_widget_herosetup_set_playerid(struct ps_widget *widget,int playerid);
int ps_widget_herosetup_get_playerid(const struct ps_widget *widget);

int ps_widget_herosetup_advance_phase(struct ps_widget *widget);
int ps_widget_herosetup_retreat_phase(struct ps_widget *widget);
int ps_widget_herosetup_get_phase(const struct ps_widget *widget);

int ps_widget_herosetup_refresh_player(struct ps_widget *widget,struct ps_player *player);

/* Slider.
 *****************************************************************************/

int ps_widget_slider_setup(struct ps_widget *widget,const char *text,int textc,int lo,int hi);
int ps_widget_slider_adjust(struct ps_widget *widget,int d);

int ps_widget_slider_set_value(struct ps_widget *widget,int v);
int ps_widget_slider_get_value(const struct ps_widget *widget);

#endif
