#ifndef AKAU_STORE_INTERNAL_H
#define AKAU_STORE_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../akau_store.h"
#include "../akau_pcm.h"
#include "../akau_instrument.h"
#include "../akau_song.h"
#include "../akau_wavegen.h"

struct akau_store_resource {
  int id;
  void *obj;
};

struct akau_store_list {
  struct akau_store_resource *v;
  int c,a;
  int contigc;
  int (*ref)(void *obj);
  void (*del)(void *obj);
};

struct akau_store {
  int refc;
  char *path;
  struct akau_store_list ipcms;
  struct akau_store_list fpcms;
  struct akau_store_list instruments;
  struct akau_store_list songs;
};

void akau_store_list_cleanup(struct akau_store_list *list);
void akau_store_list_clear(struct akau_store_list *list);
int akau_store_list_search(const struct akau_store_list *list,int id);
int akau_store_list_insert(struct akau_store_list *list,int p,int id,void *obj);
int akau_store_list_replace(struct akau_store_list *list,int p,void *obj);
int akau_store_list_get_id_by_object(const struct akau_store_list *list,const void *obj);

#endif
