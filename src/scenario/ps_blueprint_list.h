/* ps_blueprint_list.h
 */

#ifndef PS_BLUEPRINT_LIST_H
#define PS_BLUEPRINT_LIST_H

struct ps_blueprint;

struct ps_blueprint_list {
  struct ps_blueprint **v;
  int c,a;
};

struct ps_blueprint_list *ps_blueprint_list_new();
void ps_blueprint_list_del(struct ps_blueprint_list *list);

int ps_blueprint_list_clear(struct ps_blueprint_list *list);
int ps_blueprint_list_add(struct ps_blueprint_list *list,struct ps_blueprint *blueprint);
int ps_blueprint_list_remove(struct ps_blueprint_list *list,struct ps_blueprint *blueprint);

/* Examine the global resource manager.
 * Add all valid blueprints to this list, considering player count and skills.
 */
int ps_blueprint_list_match_resources(
  struct ps_blueprint_list *list,
  int playerc,uint16_t skills,
  uint8_t difficulty
);

int ps_blueprint_list_count_by_filter(const struct ps_blueprint_list *list,int (*filter)(const struct ps_blueprint *blueprint));
struct ps_blueprint *ps_blueprint_list_get_by_filter(const struct ps_blueprint_list *list,int (*filter)(const struct ps_blueprint *blueprint),int p);

#endif
