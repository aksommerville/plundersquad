#include "ps.h"
#include "ps_input_config.h"
#include "ps_input_maptm.h"
#include "ps_input_device.h"
#include "ps_input.h"
#include "os/ps_fs.h"
#include "util/ps_buffer.h"

/* Object lifecycle.
 */

struct ps_input_config *ps_input_config_new() {
  struct ps_input_config *config=calloc(1,sizeof(struct ps_input_config));
  if (!config) return 0;
  return config;
}

void ps_input_config_del(struct ps_input_config *config) {
  if (!config) return;

  if (config->maptmv) {
    while (config->maptmc-->0) ps_input_maptm_del(config->maptmv[config->maptmc]);
    free(config->maptmv);
  }

  free(config);
}

/* Clear.
 */

int ps_input_config_clear(struct ps_input_config *config) {
  if (!config) return -1;
  while (config->maptmc>0) {
    config->maptmc--;
    ps_input_maptm_del(config->maptmv[config->maptmc]);
  }
  return 0;
}

/* Install template. HANDOFF
 */

static int ps_input_config_install_maptm(struct ps_input_config *config,struct ps_input_maptm *maptm) {
  if (!config||!maptm) return -1;

  if (config->maptmc>=config->maptma) {
    int na=config->maptma+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(config->maptmv,sizeof(void*)*na);
    if (!nv) return -1;
    config->maptmv=nv;
    config->maptma=na;
  }

  config->maptmv[config->maptmc]=maptm;
  config->maptmc++;

  return 0;
}

/* Save.
 */

int ps_input_config_save(const char *path,const struct ps_input_config *config) {
  if (!path||!path[0]||!config) return -1;
  char *src=0;
  int srcc=ps_input_config_encode(&src,config);
  if (srcc<0) return -1;
  int err=ps_file_write(path,src,srcc);
  if (src) free(src);
  if (err<0) {
    ps_log(INPUT,ERROR,"%s: Failed to save input configuraiton.",path);
    return -1;
  } else {
    ps_log(INPUT,INFO,"%s: Saved input configuration.",path);
    return 0;
  }
}

/* Load.
 */
 
int ps_input_config_load(struct ps_input_config *config,const char *path) {
  if (!config||!path||!path[0]) return -1;
  char *src=0;
  int srcc=ps_file_read(&src,path);
  if (srcc<0) {
    ps_log(INPUT,ERROR,"%s: Failed to read input configuration.",path);
    return -1;
  }
  int err=ps_input_config_decode(config,src,srcc);
  if (src) free(src);
  if (err<0) {
    ps_log(INPUT,ERROR,"%s: Failed to decode input configuration.",path);
    return -1;
  } else {
    ps_log(INPUT,INFO,"%s: Loaded input configuration.",path);
    return 0;
  }
}

/* Encode.
 */
 
int ps_input_config_encode(void *dstpp,const struct ps_input_config *config) {
  if (!dstpp||!config) return -1;
  struct ps_buffer buffer={0};
  int i=0; for (;i<config->maptmc;i++) {
    struct ps_input_maptm *maptm=config->maptmv[i];
    while (1) {
      int err=ps_input_maptm_encode(buffer.v+buffer.c,buffer.a-buffer.c,maptm);
      if (err<0) {
        ps_buffer_cleanup(&buffer);
        return -1;
      }
      if (buffer.c<=buffer.a-err) {
        buffer.c+=err;
        continue;
      }
      if (ps_buffer_require(&buffer,err)<0) {
        ps_buffer_cleanup(&buffer);
        return -1;
      }
    }
  }
  *(void**)dstpp=buffer.v;
  return buffer.c;
}

/* Decode.
 */
 
int ps_input_config_decode(struct ps_input_config *config,const char *src,int srcc) {
  if (!config) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,err;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    struct ps_input_maptm *maptm=ps_input_maptm_new();
    if (!maptm) return -1;
    if ((err=ps_input_maptm_decode(maptm,src+srcp,srcc-srcp))<=0) {
      ps_input_maptm_del(maptm);
      return -1;
    }
    srcp+=err;
    if (maptm->invalid_provider) {
      ps_input_maptm_del(maptm);
    } else {
      if (ps_input_config_install_maptm(config,maptm)<0) {
        ps_input_maptm_del(maptm);
        return -1;
      }
    }
  }
  return 0;
}

/* Select template for device.
 */
 
struct ps_input_maptm *ps_input_config_select_maptm_for_device(
  const struct ps_input_config *config,const struct ps_input_device *device
) {
  if (!config||!device) return 0;
  struct ps_input_maptm *best=0;
  int bestscore=0;
  int i=0; for (;i<config->maptmc;i++) {
    struct ps_input_maptm *maptm=config->maptmv[i];
    int score=ps_input_maptm_match_device(maptm,device);
    if (score>bestscore) {
      best=maptm;
      bestscore=score;
    }
  }
  return best;
}
