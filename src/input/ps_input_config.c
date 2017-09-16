#include "ps_input_internal.h"

/* New.
 */

struct ps_input_config *ps_input_config_new() {
  struct ps_input_config *config=calloc(1,sizeof(struct ps_input_config));
  if (!config) return 0;

  return config;
}

/* Delete.
 */
 
void ps_input_config_del(struct ps_input_config *config) {
  if (!config) return;

  if (config->path) free(config->path);

  free(config);
}

/* Trivial accessors.
 */

int ps_input_config_set_path(struct ps_input_config *config,const char *src,int srcc) {
  if (!config) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>2048) return -1; // Arbitrary sanity limit.
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (config->path) free(config->path);
  config->path=nv;
  return 0;
}

const char *ps_input_config_get_path(const struct ps_input_config *config) {
  if (!config) return 0;
  return config->path;
}
