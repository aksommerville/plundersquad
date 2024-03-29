#include "ps.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "ps_blueprint.h"
#include "ps_blueprint_list.h"

/* New.
 */

struct ps_blueprint_list *ps_blueprint_list_new() {
  struct ps_blueprint_list *list=calloc(1,sizeof(struct ps_blueprint_list));
  if (!list) return 0;

  return list;
}

/* Delete.
 */
 
void ps_blueprint_list_del(struct ps_blueprint_list *list) {
  if (!list) return;
  ps_blueprint_list_clear(list);
  if (list->v) free(list->v);
  free(list);
}

/* Clear.
 */

int ps_blueprint_list_clear(struct ps_blueprint_list *list) {
  if (!list) return -1;
  while (list->c>0) {
    list->c--;
    ps_blueprint_del(list->v[list->c]);
  }
  return 0;
}

/* Add.
 */
 
int ps_blueprint_list_add(struct ps_blueprint_list *list,struct ps_blueprint *blueprint) {
  if (!list) return -1;
  if (!blueprint) return -1;

  int i; for (i=0;i<list->c;i++) if (list->v[i]==blueprint) return 0;

  if (list->c>=list->a) {
    int na=list->a+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(list->v,sizeof(void*)*na);
    if (!nv) return -1;
    list->v=nv;
    list->a=na;
  }

  if (ps_blueprint_ref(blueprint)<0) return -1;

  list->v[list->c++]=blueprint;

  return 1;
}

/* Remove.
 */
 
int ps_blueprint_list_remove(struct ps_blueprint_list *list,struct ps_blueprint *blueprint) {
  if (!list||!blueprint) return -1;
  int i; for (i=0;i<list->c;i++) if (list->v[i]==blueprint) {
    list->c--;
    memmove(list->v+i,list->v+i+1,sizeof(void*)*(list->c-i));
    ps_blueprint_del(blueprint);
    return 1;
  }
  return 0;
}

int ps_blueprint_list_remove_at(struct ps_blueprint_list *list,int p) {
  if (!list) return -1;
  if ((p<0)||(p>=list->c)) return -1;
  struct ps_blueprint *blueprint=list->v[p];
  list->c--;
  memmove(list->v+p,list->v+p+1,sizeof(void*)*(list->c-p));
  ps_blueprint_del(blueprint);
  return 0;
}

/* Extra hook to ignore blueprints.
 * This function should normally just return zero.
 * Patch in here to generate an otherwise-normal scenario with specific blueprints.
 * Unlike the special setup in ps_main.c, this is creating a real scenario: You must provide HERO and TREASURE blueprints.
 */

static int ps_DEBUG_ignore_blueprint(struct ps_blueprint *blueprint,int id) {
  return 0;
}

/* Match resources.
 */
 
int ps_blueprint_list_match_resources(
  struct ps_blueprint_list *list,
  int playerc,uint16_t skills,
  uint8_t difficulty
) {
  if (!list) return -1;
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_BLUEPRINT);
  if (!restype) return -1;
  int i; for (i=0;i<restype->resc;i++) {
    struct ps_blueprint *blueprint=restype->resv[i].obj;
    uint8_t bpdiff;
    
    int basedifficulty=ps_blueprint_get_base_difficulty(blueprint);
    if (basedifficulty>difficulty) {
      //ps_log(GENERATOR,DEBUG,"Rejecting blueprint:%d due to difficulty %d>%d",restype->resv[i].id,basedifficulty,difficulty);
      continue;
    }
    
    if (ps_blueprint_is_solvable(&bpdiff,blueprint,playerc,skills)) {
      if (difficulty>=bpdiff) {

        /* Extra rule: If the blueprint contains any HERO POI, it must have enough for the requested player count.
         * Further down in the generator, we will assume that any HERO POI indicates a valid home screen.
         */
        if (playerc>1) {
          int poic=ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_HERO);
          if ((poic>0)&&(poic<playerc)) continue;
        }

        if (ps_DEBUG_ignore_blueprint(blueprint,restype->resv[i].id)) continue;

        if (ps_blueprint_list_add(list,blueprint)<0) return -1;
      }
    }
  }
  return 0;
}

/* Filter list generically.
 */

int ps_blueprint_list_count_by_filter(const struct ps_blueprint_list *list,int (*filter)(const struct ps_blueprint *blueprint)) {
  if (!list) return -1;
  if (!filter) return -1;
  int i=0,c=0; for (;i<list->c;i++) {
    struct ps_blueprint *blueprint=list->v[i];
    if (filter(blueprint)) c++;
  }
  return c;
}

struct ps_blueprint *ps_blueprint_list_get_by_filter(const struct ps_blueprint_list *list,int (*filter)(const struct ps_blueprint *blueprint),int p) {
  if (!list) return 0;
  if (!filter) return 0;
  if (p<0) return 0;
  int i=0; for (;i<list->c;i++) {
    struct ps_blueprint *blueprint=list->v[i];
    if (filter(blueprint)) {
      if (!p--) return blueprint;
    }
  }
  return 0;
}

int ps_blueprint_list_apply_filter(struct ps_blueprint_list *dst,const struct ps_blueprint_list *src,int (*filter)(const struct ps_blueprint *blueprint)) {
  if (!dst||!src||!filter) return -1;
  int i=0; for (;i<src->c;i++) {
    struct ps_blueprint *blueprint=src->v[i];
    if (filter(blueprint)) {
      if (ps_blueprint_list_add(dst,blueprint)<0) return -1;
    }
  }
  return 0;
}
