#include "ps.h"
#include "ps_file_list.h"
#include "os/ps_fs.h"
#include <sys/stat.h>

/* Object definition.
 */

struct ps_file_list {
  int refc;
  char **pathv;
  int pathc,patha;
};

/* Object lifecycle.
 */

struct ps_file_list *ps_file_list_new() {
  struct ps_file_list *list=calloc(1,sizeof(struct ps_file_list));
  if (!list) return 0;

  list->refc=1;

  return list;
}

void ps_file_list_del(struct ps_file_list *list) {
  if (!list) return;
  if (list->refc-->1) return;

  if (list->pathv) {
    while (list->pathc-->0) {
      if (list->pathv[list->pathc]) free(list->pathv[list->pathc]);
    }
    free(list->pathv);
  }

  free(list);
}

int ps_file_list_ref(struct ps_file_list *list) {
  if (!list) return -1;
  if (list->refc<1) return -1;
  if (list->refc==INT_MAX) return -1;
  list->refc++;
  return 0;
}

/* Simple direct readers.
 */

int ps_file_list_count(const struct ps_file_list *list) {
  if (!list) return -1;
  return list->pathc;
}

const char *ps_file_list_get_by_index(const struct ps_file_list *list,int p) {
  if (!list) return 0;
  if ((p<0)||(p>=list->pathc)) return 0;
  return list->pathv[p];
}

/* Path refers to an existing, regular file.
 */

static int ps_file_list_test_existing_file(const char *path) {
  struct stat st;
  if (stat(path,&st)<0) return 0;
  if (!S_ISREG(st.st_mode)) return 0;
  return 1;
}

/* Path refers to an existing directory.
 */

static int ps_file_list_test_existing_directory(const char *path) {
  struct stat st;
  if (stat(path,&st)<0) return 0;
  if (!S_ISDIR(st.st_mode)) return 0;
  return 1;
}

/* Path refers to an existing file, either regular or directory.
 */

static int ps_file_list_test_existing_file_or_directory(const char *path) {
  struct stat st;
  if (stat(path,&st)<0) return 0;
  if (S_ISREG(st.st_mode)) return 1;
  if (S_ISDIR(st.st_mode)) return 1;
  return 0;
}

/* Path's dirname is an existing directory.
 */

static int ps_file_list_test_existing_parent(const char *path) {
  char dirname[1024];
  int dirnamec=ps_file_dirname(dirname,sizeof(dirname),path,-1);
  if ((dirnamec<1)||(dirnamec>=sizeof(dirname))) return 0;
  struct stat st;
  if (stat(dirname,&st)<0) return 0;
  if (!S_ISDIR(st.st_mode)) return 0;
  return 1;
}

/* Smart readers -- first path matching some criterion.
 */

const char *ps_file_list_get_first_existing_file(const struct ps_file_list *list) {
  if (!list) return 0;
  int i=0; for (;i<list->pathc;i++) {
    if (ps_file_list_test_existing_file(list->pathv[i])) return list->pathv[i];
  }
  return 0;
}

const char *ps_file_list_get_first_existing_directory(const struct ps_file_list *list) {
  if (!list) return 0;
  int i=0; for (;i<list->pathc;i++) {
    if (ps_file_list_test_existing_directory(list->pathv[i])) return list->pathv[i];
  }
  return 0;
}

const char *ps_file_list_get_first_existing_file_or_directory(const struct ps_file_list *list) {
  if (!list) return 0;
  int i=0; for (;i<list->pathc;i++) {
    if (ps_file_list_test_existing_file_or_directory(list->pathv[i])) return list->pathv[i];
  }
  return 0;
}

const char *ps_file_list_get_first_existing_parent(const struct ps_file_list *list) {
  if (!list) return 0;
  int i=0; for (;i<list->pathc;i++) {
    if (ps_file_list_test_existing_parent(list->pathv[i])) return list->pathv[i];
  }
  return 0;
}

/* Add to list.
 */

static int ps_file_list_require(struct ps_file_list *list) {
  if (list->pathc<list->patha) return 0;
  int na=list->patha+8;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(list->pathv,sizeof(void*)*na);
  if (!nv) return -1;
  list->pathv=nv;
  list->patha=na;
  return 0;
}

int ps_file_list_add(struct ps_file_list *list,int p,const char *src,int srcc) {
  if (!list) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>=1024) return -1;
  if ((p<0)||(p>list->pathc)) p=list->pathc;

  if (ps_file_list_require(list)<0) return -1;
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;

  memmove(list->pathv+p+1,list->pathv+p,sizeof(void*)*(list->pathc-p));
  list->pathv[p]=nv;
  list->pathc++;

  return 0;
}
