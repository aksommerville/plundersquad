#include "ps.h"
#include "gui/ps_gui.h"
#include "util/ps_geometry.h"
#include "game/ps_game.h"

static int ps_assemble_is_complete(const struct ps_page *page);
static int ps_assemble_setup_game(struct ps_page *page);

/* Page definition.
 */

struct ps_page_assemble {
  struct ps_page hdr;
  struct ps_widget *heroselect;
  int input_enabled;
};

#define PAGE ((struct ps_page_assemble*)page)

/* Delete.
 */

static void _ps_assemble_del(struct ps_page *page) {
  ps_widget_del(PAGE->heroselect);
}

/* Initialize.
 */

static int _ps_assemble_init(struct ps_page *page) {

  page->root->bgrgba=0x200060ff;

  struct ps_widget *packer=ps_widget_spawn(page->root,&ps_widget_type_packer);
  if (!packer) return -1;
  if (ps_widget_packer_setup(packer,PS_AXIS_VERT,PS_ALIGN_CENTER,PS_ALIGN_CENTER,0,10)<0) return -1;

  struct ps_widget *label=ps_widget_spawn(packer,&ps_widget_type_label);
  if (!label) return -1;
  label->fgrgba=0xffff00ff;
  if (ps_widget_label_set_text(label,"Plunder Squad",-1)<0) return -1;

  if (!(PAGE->heroselect=ps_widget_new(&ps_widget_type_heroselect))) return -1;
  if (ps_widget_add_child(packer,PAGE->heroselect)<0) return -1;

  return 0;
}

/* Unified input events.
 */

static int _ps_assemble_move_cursor(struct ps_page *page,int dx,int dy) {
  return 0;
}

static int _ps_assemble_activate(struct ps_page *page) {
  return 0;
}

static int _ps_assemble_submit(struct ps_page *page) {
  return 0;
}

static int _ps_assemble_cancel(struct ps_page *page) {
  return 0;
}

/* Update.
 */

static int _ps_assemble_update(struct ps_page *page) {

  /* Enabling input on the heroselect widget is deferred until GUI construction complete. */
  if (!PAGE->input_enabled) {
    if (ps_widget_heroselect_enable_input(PAGE->heroselect)<0) return -1;
    PAGE->input_enabled=1;
  }

  if (ps_assemble_is_complete(page)) {
    ps_log(GUI,DEBUG,"**** assembly page complete ****");
    if (ps_assemble_setup_game(page)<0) return -1;
    if (ps_gui_load_page_sconfig(page->gui)<0) return -1;
    return 0;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ps_page_type ps_page_type_assemble={
  .name="assemble",
  .objlen=sizeof(struct ps_page_assemble),
  .init=_ps_assemble_init,
  .del=_ps_assemble_del,

  .move_cursor=_ps_assemble_move_cursor,
  .activate=_ps_assemble_activate,
  .submit=_ps_assemble_submit,
  .cancel=_ps_assemble_cancel,

  .update=_ps_assemble_update,
};

/* Check completion.
 * We are complete when:
 *   - At least one herosetup widget exists.
 *   - At least one herosetup widget is in FINISHED state.
 *   - No herosetup widget is in JOINED state.
 */
 
static int ps_assemble_is_complete(const struct ps_page *page) {
  if (!page||!PAGE->heroselect||(PAGE->heroselect->type!=&ps_widget_type_heroselect)) return 0;
  
  struct ps_widget **herosetupv=PAGE->heroselect->childv;
  int herosetupc=PAGE->heroselect->childc;
  if (herosetupc<1) return 0;
  int finishedc=0;
  int i=herosetupc; while (i-->0) {
    switch (ps_widget_herosetup_get_phase(herosetupv[i])) {
      case PS_HEROSETUP_PHASE_INIT: break;
      case PS_HEROSETUP_PHASE_EDIT: return 0;
      case PS_HEROSETUP_PHASE_READY: finishedc++; break;
    }
  }
  if (finishedc) return 1;

  return 0;
}

/* Setup game.
 */
 
static int ps_assemble_setup_game(struct ps_page *page) {
  struct ps_game *game=ps_gui_get_game(page->gui);
  if (!game) return -1;

  /* On second thought, I'm not sure there's anything for us to do here.
   * The player configuration will happen in real time during the assemble page.
   * Setting length and difficulty will happen on the next page.
   */

  return 0;
}
