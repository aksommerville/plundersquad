/* ps_page.h
 * Single screen of GUI content.
 */

#ifndef PS_PAGE_H
#define PS_PAGE_H

struct ps_widget;
struct ps_page;

struct ps_page_type {
  const char *name;
  int objlen;

  int (*init)(struct ps_page *page);
  void (*del)(struct ps_page *page);

};

struct ps_page {
  const struct ps_page_type *type;
  int refc;
  struct ps_widget *root;
};

struct ps_page *ps_page_new(const struct ps_page_type *type);
void ps_page_del(struct ps_page *page);
int ps_page_ref(struct ps_page *page);

// Initialization flow:
extern const struct ps_page_type ps_page_type_assemble; // First page. Let players join in and configure themselves.
extern const struct ps_page_type ps_page_type_sconfig; // Configure scenario (difficulty and length).
extern const struct ps_page_type ps_page_type_pconfig; // Configure players, for things that don't affect scenario (eg colors).

// Game in progress:
extern const struct ps_page_type ps_page_type_pause;
extern const struct ps_page_type ps_page_type_debug;

// Game over:
extern const struct ps_page_type ps_page_type_gameover;

#endif
