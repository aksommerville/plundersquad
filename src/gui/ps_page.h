/* ps_page.h
 * Single screen of GUI content.
 */

#ifndef PS_PAGE_H
#define PS_PAGE_H

struct ps_widget;
struct ps_page;
struct ps_game;

struct ps_page_type {
  const char *name;
  int objlen;

  int (*init)(struct ps_page *page);
  void (*del)(struct ps_page *page);

  /* Unified input.
   * If you implement both, (activate) is A and (submit) is START.
   * If you only implement one, it is both A and START.
   */
  int (*move_cursor)(struct ps_page *page,int dx,int dy);
  int (*activate)(struct ps_page *page);
  int (*submit)(struct ps_page *page);
  int (*cancel)(struct ps_page *page);

  int (*update)(struct ps_page *page);

};

struct ps_page {
  const struct ps_page_type *type;
  int refc;
  struct ps_widget *root;
  struct ps_gui *gui; // WEAK; set only for loaded page.
  struct ps_widget **modalv; // Above root, overriding it.
  int modalc,modala;
};

struct ps_page *ps_page_new(const struct ps_page_type *type);
void ps_page_del(struct ps_page *page);
int ps_page_ref(struct ps_page *page);

int ps_page_update(struct ps_page *page);

struct ps_game *ps_page_get_game(const struct ps_page *page);

int ps_page_move_cursor(struct ps_page *page,int dx,int dy);
int ps_page_activate(struct ps_page *page);
int ps_page_submit(struct ps_page *page);
int ps_page_cancel(struct ps_page *page);

int ps_page_push_modal(struct ps_page *page,struct ps_widget *modal);
int ps_page_pop_modal(struct ps_page *page);

// Initialization flow:
extern const struct ps_page_type ps_page_type_assemble; // First page. Let players join in and configure themselves.
extern const struct ps_page_type ps_page_type_sconfig; // Configure scenario (difficulty and length).
extern const struct ps_page_type ps_page_type_pconfig; // Configure players, for things that don't affect scenario (eg colors). XXX unused

// Game in progress:
extern const struct ps_page_type ps_page_type_pause;
extern const struct ps_page_type ps_page_type_debug;

// Game over:
extern const struct ps_page_type ps_page_type_gameover;

// Editor:
extern const struct ps_page_type ps_page_type_edithome;
extern const struct ps_page_type ps_page_type_editsfx;
extern const struct ps_page_type ps_page_type_editsong;
extern const struct ps_page_type ps_page_type_editblueprint;
extern const struct ps_page_type ps_page_type_editsprdef;
extern const struct ps_page_type ps_page_type_editplrdef;

#endif
