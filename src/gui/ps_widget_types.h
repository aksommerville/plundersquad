/* ps_widget_types.h
 * Public header for specific widget types.
 */

#ifndef PS_WIDGET_TYPES_H
#define PS_WIDGET_TYPES_H

struct ps_input_device;
struct ps_player;
struct akau_ipcm;
struct akau_song;

extern const struct ps_widget_type ps_widget_type_root; // Only for pages' root widgets. One full-size child.
extern const struct ps_widget_type ps_widget_type_label; // Single row of text.
extern const struct ps_widget_type ps_widget_type_packer; // Single column or row of widgets.
extern const struct ps_widget_type ps_widget_type_menu; // Vertical packer of labels with selection.
extern const struct ps_widget_type ps_widget_type_heroselect; // Container and controller for multiple herosetup widgets.
extern const struct ps_widget_type ps_widget_type_herosetup; // Panel for one input device, to click in and set up.
extern const struct ps_widget_type ps_widget_type_slider; // Label and horiztonal indicator, for use in menus.
extern const struct ps_widget_type ps_widget_type_resedit; // Resource editor, agnostic to data type.
extern const struct ps_widget_type ps_widget_type_reseditmenu; // Menu bar under resedit.
extern const struct ps_widget_type ps_widget_type_editsfx; // Sound effect editor.
extern const struct ps_widget_type ps_widget_type_editsfxchan; // Channel row in editsfx.
extern const struct ps_widget_type ps_widget_type_editsfxgraph; // Single graph view in editsfx.
extern const struct ps_widget_type ps_widget_type_editsong; // Song editor.

/* Root.
 *****************************************************************************/

int ps_widget_root_set_page(struct ps_widget *widget,struct ps_page *page);
struct ps_page *ps_widget_root_get_page(const struct ps_widget *widget);

/* Label.
 *****************************************************************************/

int ps_widget_label_set_text(struct ps_widget *widget,const char *src,int srcc);

// Convenience to initialize a label and add to parent. Returns WEAK reference on success.
struct ps_widget *ps_widget_spawn_label(struct ps_widget *parent,const char *src,int srcc,uint32_t rgba);

int ps_widget_label_set_click_cb(
  struct ps_widget *widget,
  int (*cb)(struct ps_widget *widget,void *userdata),
  void *userdata,
  void (*userdata_del)(void *userdata)
);

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

/* ResEdit.
 *****************************************************************************/

/* Provides hooks from the controller page, for resedit widget to call.
 */
struct ps_resedit_delegate {
  int (*res_new)(struct ps_page *page); // => index or <0=error
  int (*res_del)(struct ps_page *page,int index);
  int (*res_count)(struct ps_page *page);
  int (*res_load)(struct ps_page *page,int index);
  int (*res_save)(struct ps_page *page,int index);
};

int ps_widget_resedit_set_delegate(struct ps_widget *widget,const struct ps_resedit_delegate *delegate);

int ps_widget_resedit_set_editor(struct ps_widget *widget,struct ps_widget *editor);
struct ps_widget *ps_widget_resedit_get_editor(const struct ps_widget *widget);

int ps_widget_reseditmenu_set_name(struct ps_widget *widget,const char *text,int textc);

struct ps_widget *ps_widget_reseditmenu_add_menu(
  struct ps_widget *widget,
  const char *text,int textc,
  int (*cb)(struct ps_widget *label,void *userdata),
  void *userdata,
  void (*userdata_del)(void *userdata)
);

/* Editsfx.
 *****************************************************************************/

int ps_widget_editsfx_set_ipcm(struct ps_widget *widget,struct akau_ipcm *ipcm);
struct akau_ipcm *ps_widget_editsfx_get_ipcm(const struct ps_widget *widget);
int ps_widget_editsfx_set_path(struct ps_widget *widget,const char *path);
struct ps_iwg *ps_widget_editsfx_get_iwg(const struct ps_widget *widget);

int ps_widget_editsfxchan_set_chanid(struct ps_widget *widget,int chanid);

int ps_widget_editsfxgraph_set_field(struct ps_widget *widget,int chanid,int k);

/* Editsong.
 *****************************************************************************/

int ps_widget_editsong_set_song(struct ps_widget *widget,struct akau_song *song);
struct akau_song *ps_widget_editsong_get_song(const struct ps_widget *widget);
int ps_widget_editsong_set_path(struct ps_widget *widget,const char *path);

#endif
