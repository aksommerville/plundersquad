#include "akau_store_internal.h"

/* List primitives.
 */
 
void akau_store_list_cleanup(struct akau_store_list *list) {
  if (list->v) {
    while (list->c-->0) list->del(list->v[list->c].obj);
    free(list->v);
  }
  memset(list,0,sizeof(struct akau_store_list));
}

void akau_store_list_clear(struct akau_store_list *list) {
  while (list->c>0) {
    list->c--;
    list->del(list->v[list->c].obj);
  }
  list->contigc=0;
}

int akau_store_list_search(const struct akau_store_list *list,int id) {
  if (id<1) return -1;
  if (id<=list->contigc) return id-1;
  if (list->c<1) return -1;
  if (id>list->v[list->c-1].id) return -list->c-1;
  int lo=list->contigc,hi=list->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (id<list->v[ck].id) hi=ck;
    else if (id>list->v[ck].id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int akau_store_list_insert(struct akau_store_list *list,int p,int id,void *obj) {
  if ((p<0)||(p>list->c)) return -1;
  if (id<1) return -1;
  if (p&&(id<=list->v[p-1].id)) return -1;
  if ((p<list->c)&&(id>=list->v[p].id)) return -1;

  if (list->c>=list->a) {
    int na=list->a+16;
    if (na>INT_MAX/sizeof(struct akau_store_resource)) return -1;
    void *nv=realloc(list->v,sizeof(struct akau_store_resource)*na);
    if (!nv) return -1;
    list->v=nv;
    list->a=na;
  }

  if (list->ref(obj)<0) return -1;

  struct akau_store_resource *resource=list->v+p;
  memmove(resource+1,resource,sizeof(struct akau_store_resource)*(list->c-p));
  list->c++;

  resource->id=id;
  resource->obj=obj;

  while ((list->contigc<list->c)&&(list->v[list->contigc].id==list->contigc+1)) list->contigc++;

  return 0;
}

int akau_store_list_replace(struct akau_store_list *list,int p,void *obj) {
  if ((p<0)||(p>=list->c)) return -1;
  if (!obj) return -1;
  if (list->ref(obj)<0) return -1;
  list->del(list->v[p].obj);
  list->v[p].obj=obj;
  return 0;
}

int akau_store_list_get_id_by_object(const struct akau_store_list *list,const void *obj) {
  if (!list) return -1;
  const struct akau_store_resource *res=list->v;
  int i=list->c; for (;i-->0;res++) {
    if (res->obj==obj) return res->id;
  }
  return -1;
}

/* New store.
 */

struct akau_store *akau_store_new() {
  struct akau_store *store=calloc(1,sizeof(struct akau_store));
  if (!store) return 0;

  store->refc=1;

  #define INITLIST(tag) \
    store->tag##s.ref=(void*)akau_##tag##_ref; \
    store->tag##s.del=(void*)akau_##tag##_del; 
  INITLIST(ipcm)
  INITLIST(fpcm)
  INITLIST(instrument)
  INITLIST(song)
  #undef INITLIST

  return store;
}

/* Delete store.
 */
 
void akau_store_del(struct akau_store *store) {
  if (!store) return;
  if (store->refc-->1) return;

  if (store->path) free(store->path);

  akau_store_list_cleanup(&store->ipcms);
  akau_store_list_cleanup(&store->fpcms);
  akau_store_list_cleanup(&store->instruments);
  akau_store_list_cleanup(&store->songs);

  free(store);
}

/* Retain.
 */
 
int akau_store_ref(struct akau_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Clear.
 */

int akau_store_clear(struct akau_store *store) {
  if (!store) return -1;
  akau_store_list_clear(&store->ipcms);
  akau_store_list_clear(&store->fpcms);
  akau_store_list_clear(&store->instruments);
  akau_store_list_clear(&store->songs);
  return 0;
}

/* Get or add resource.
 */

#define LISTACCESSORS(tag) \
  struct akau_##tag *akau_store_get_##tag(const struct akau_store *store,int id) { \
    if (!store) return 0; \
    int p=akau_store_list_search(&store->tag##s,id); \
    if (p<0) return 0; \
    return store->tag##s.v[p].obj; \
  } \
  int akau_store_add_##tag(struct akau_store *store,struct akau_##tag *obj,int id) { \
    if (!store) return -1; \
    if (!obj) return -1; \
    if (id<1) return -1; \
    int p=akau_store_list_search(&store->tag##s,id); \
    if (p>=0) { \
      if (store->tag##s.v[p].obj==obj) return 0; \
      return -1; \
    } \
    p=-p-1; \
    if (akau_store_list_insert(&store->tag##s,p,id,obj)<0) return -1; \
    return 0; \
  } \
  int akau_store_replace_##tag(struct akau_store *store,struct akau_##tag *obj,int id) { \
    if (!store) return -1; \
    if (!obj) return -1; \
    if (id<1) return -1; \
    int p=akau_store_list_search(&store->tag##s,id); \
    if (p<0) return -1; \
    if (akau_store_list_replace(&store->tag##s,p,obj)<0) return -1; \
    return 0; \
  } \
  int akau_store_count_##tag(const struct akau_store *store) { \
    if (!store) return 0; \
    return store->tag##s.c; \
  } \
  struct akau_##tag *akau_store_get_##tag##_by_index(const struct akau_store *store,int p) { \
    if (!store) return 0; \
    if ((p<0)||(p>=store->tag##s.c)) return 0; \
    return store->tag##s.v[p].obj; \
  } \
  int akau_store_get_##tag##_id_by_index(const struct akau_store *store,int p) { \
    if (!store) return 0; \
    if ((p<0)||(p>=store->tag##s.c)) return 0; \
    return store->tag##s.v[p].id; \
  } \
  int akau_store_get_unused_##tag##_id(int *id,int *p,const struct akau_store *store) { \
    if (!store) return -1; \
    if (id) *id=store->tag##s.contigc+1; \
    if (p) *p=store->tag##s.contigc; \
    return 0; \
  } \
  int akau_store_get_##tag##_id_by_object(const struct akau_store *store,const struct akau_##tag *obj) { \
    if (!store) return -1; \
    return akau_store_list_get_id_by_object(&store->tag##s,obj); \
  }

LISTACCESSORS(ipcm)
LISTACCESSORS(fpcm)
LISTACCESSORS(instrument)
LISTACCESSORS(song)

#undef LISTACCESSORS

/* Relink all songs.
 */
 
int akau_store_relink_songs(struct akau_store *store) {
  if (!store) return -1;
  struct akau_store_resource *res=store->songs.v;
  int i=store->songs.c;
  for (;i-->0;res++) {
    struct akau_song *song=res->obj;
    if (akau_song_unlink(song)<0) return -1;
    if (akau_song_link(song,store)<0) return -1;
  }
  return 0;
}
