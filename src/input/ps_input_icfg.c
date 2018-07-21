#include "ps.h"
#include "ps_input_icfg.h"
#include "ps_input_device.h"
#include "ps_input_premap.h"
#include "ps_input_map.h"
#include "ps_input_maptm.h"
#include "ps_input_button.h"
#include "ps_input_config.h"
#include "ps_input.h"
#include "os/ps_clockassist.h"

#define PS_ICFG_PHASE_INIT      0 /* Waiting for first stroke. */
#define PS_ICFG_PHASE_HOLD1     1 /* Waiting to release first stroke. */
#define PS_ICFG_PHASE_CONFIRM   2 /* Waiting for second stroke. */
#define PS_ICFG_PHASE_HOLD2     3 /* Waiting to release second stroke. */

/* Object lifecycle.
 */

struct ps_input_icfg *ps_input_icfg_new(struct ps_input_device *device) {
  struct ps_input_icfg *icfg=calloc(1,sizeof(struct ps_input_icfg));
  if (!icfg) return 0;

  icfg->refc=1;

  if (ps_input_icfg_set_device(icfg,device)<0) {
    ps_input_icfg_del(icfg);
    return 0;
  }

  icfg->mapv[0].dstbtnid=PS_PLRBTN_UP;
  icfg->mapv[1].dstbtnid=PS_PLRBTN_DOWN;
  icfg->mapv[2].dstbtnid=PS_PLRBTN_LEFT;
  icfg->mapv[3].dstbtnid=PS_PLRBTN_RIGHT;
  icfg->mapv[4].dstbtnid=PS_PLRBTN_A;
  icfg->mapv[5].dstbtnid=PS_PLRBTN_B;
  icfg->mapv[6].dstbtnid=PS_PLRBTN_START;

  icfg->interlude_us=50000;

  return icfg;
}

void ps_input_icfg_del(struct ps_input_icfg *icfg) {
  if (!icfg) return;
  if (icfg->refc-->1) return;

  ps_input_device_del(icfg->device);

  free(icfg);
}

int ps_input_icfg_ref(struct ps_input_icfg *icfg) {
  if (!icfg) return -1;
  if (icfg->refc<1) return -1;
  if (icfg->refc==INT_MAX) return -1;
  icfg->refc++;
  return 0;
}

/* Accessors.
 */

int ps_input_icfg_set_device(struct ps_input_icfg *icfg,struct ps_input_device *device) {
  if (!icfg) return -1;
  if (device) {
    if (ps_input_premap_build_for_device(device)<0) return -1;
    if (ps_input_device_ref(device)<0) return -1;
    ps_input_device_del(icfg->device);
    icfg->device=device;
  } else {
    ps_input_device_del(icfg->device);
    icfg->device=0;
  }
  return 0;
}

/* Work in progress state.
 */
 
int ps_input_icfg_is_ready(const struct ps_input_icfg *icfg) {
  if (!icfg) return 0;
  if (icfg->mapc==PS_INPUT_ICFG_MAP_COUNT) return 1;
  return 0;
}

int ps_input_icfg_get_current_button(const struct ps_input_icfg *icfg) {
  if (!icfg) return 0;
  if (icfg->mapc<0) return 0;
  if (icfg->mapc>=PS_INPUT_ICFG_MAP_COUNT) return 0;
  return icfg->mapv[icfg->mapc].dstbtnid;
}

int ps_input_icfg_get_qualifier(const struct ps_input_icfg *icfg) {
  if (!icfg) return PS_ICFG_QUALIFIER_INVALID;
  if (icfg->mapc<0) return PS_ICFG_QUALIFIER_INVALID;
  if (icfg->mapc>=PS_INPUT_ICFG_MAP_COUNT) return PS_ICFG_QUALIFIER_INVALID;
  switch (icfg->collection_phase) {
    case PS_ICFG_PHASE_INIT:
    case PS_ICFG_PHASE_HOLD1: {
        if (icfg->mismatch) return PS_ICFG_QUALIFIER_MISMATCH;
        return PS_ICFG_QUALIFIER_COLLECT;
      }
    case PS_ICFG_PHASE_CONFIRM:
    case PS_ICFG_PHASE_HOLD2: {
        return PS_ICFG_QUALIFIER_REPEAT;
      }
  }
  return PS_ICFG_QUALIFIER_INVALID;
}
 
int ps_input_icfg_restart(struct ps_input_icfg *icfg) {
  if (!icfg) return -1;
  icfg->mapc=0;
  icfg->srcbtnid=0;
  icfg->collection_phase=0;
  return 0;
}

/* Discard anything gathered for the current button.
 * eg if the user hits a different button while awaiting confirmation.
 */

static int ps_input_icfg_discard_collection(struct ps_input_icfg *icfg) {
  icfg->srcbtnid=0;
  icfg->collection_phase=PS_ICFG_PHASE_INIT;
  icfg->value=0;
  return 0;
}

/* Accept the current button and advance state.
 */

static int ps_input_icfg_accept_collection(struct ps_input_icfg *icfg,struct ps_input_btncfg *btncfg) {
  if (icfg->mapc>=PS_INPUT_ICFG_MAP_COUNT) return -1;
  
  struct ps_input_icfg_map *map=icfg->mapv+icfg->mapc++;
  map->srcbtnid=icfg->srcbtnid;
  map->nvalue=icfg->value;

  switch (ps_input_btncfg_describe(btncfg)) {

    case PS_BTNCFG_TWOSTATE: {
        if (icfg->value<0) return -1;
        map->srclo=1;
        map->srchi=btncfg->hi;
      } break;
    
    case PS_BTNCFG_ONEWAY: {
        if (icfg->value<0) return -1;
        map->srclo=(btncfg->hi>>1)+1;
        map->srchi=btncfg->hi;
      } break;

    case PS_BTNCFG_TWOWAY: {
        int mid=(btncfg->lo+btncfg->hi)>>1;
        if (icfg->value<0) {
          map->srclo=btncfg->lo;
          map->srchi=(btncfg->lo+mid)>>1;
          if (map->srchi>=mid) map->srchi=mid-1;
        } else {
          map->srclo=(btncfg->hi+mid)>>1;
          map->srchi=btncfg->hi;
          if (map->srclo<=mid) map->srclo=mid+1;
        }
      } break;

    default: return -1;
  }

  icfg->srcbtnid=0;
  icfg->collection_phase=PS_ICFG_PHASE_INIT;
  icfg->mismatch=0;
  icfg->value=0;

  if (icfg->interlude_us>0) {
    icfg->delay=ps_time_now()+icfg->interlude_us;
  }

  //XXX Skip 'B'
  if (0) {
    if (icfg->mapc==5) {
      icfg->mapv[5].srcbtnid=0;
      icfg->mapv[5].srclo=1;
      icfg->mapv[5].srchi=1;
      icfg->mapv[5].nvalue=0;
      icfg->mapc++;
    }
  }

  return 0;
}

/* Normalize input value based on btncfg limits.
 */

static int ps_input_icfg_normalize_value(const struct ps_input_btncfg *btncfg,int value) {
  switch (ps_input_btncfg_describe(btncfg)) {
    case PS_BTNCFG_TWOSTATE: return value?1:0;
    case PS_BTNCFG_ONEWAY: return (value>(btncfg->hi>>1))?1:0;
    case PS_BTNCFG_TWOWAY: {
        int mid=(btncfg->lo+btncfg->hi)>>1;
        int threshlo=(btncfg->lo+mid)>>1;
        int threshhi=(btncfg->hi+mid)>>1;
        if (threshlo>=mid) threshlo=mid-1;
        if (threshhi<=mid) threshhi=mid+1;
        if (value<=threshlo) return -1;
        if (value>=threshhi) return 1;
        return 0;
      }
  }
  return -2;
}

/* Check whether we already have a map for a given button.
 */

static int ps_input_icfg_already_mapping_event(const struct ps_input_icfg *icfg,int btnid,int nvalue) {
  const struct ps_input_icfg_map *map=icfg->mapv;
  int i=0; for (;i<icfg->mapc;i++,map++) {
    if (map->srcbtnid!=btnid) continue;
    if (map->nvalue!=nvalue) continue;
    return 1;
  }
  return 0;
}

/* Receive event.
 */
 
int ps_input_icfg_event(struct ps_input_icfg *icfg,int btnid,int value) {
  if (!icfg) return -1;
  if (!icfg->device) return 0;
  if ((icfg->mapc<0)||(icfg->mapc>=PS_INPUT_ICFG_MAP_COUNT)) return 0;

  /* Are we blacked out due to a recent event? */
  if (icfg->delay) {
    int64_t now=ps_time_now();
    if (now<icfg->delay) return 0;
    icfg->delay=0;
  }

  struct ps_input_btncfg *btncfg=ps_input_premap_get(icfg->device->premap,btnid);
  if (!btncfg) {
    // Button not known or deliberately ignored by premap.
    return 0;
  }

  int nvalue=ps_input_icfg_normalize_value(btncfg,value);
  if ((nvalue<-1)||(nvalue>1)) return 0;

  //ps_log(INPUT,INFO,"btnid=%08x value=%d[%d] phase=%d expectbtnid=%d expectvalue=%d",btnid,value,nvalue,icfg->collection_phase,icfg->srcbtnid,icfg->value);

  switch (icfg->collection_phase) {

    case PS_ICFG_PHASE_INIT: {
        if (!nvalue) return 0;
        if (ps_input_icfg_already_mapping_event(icfg,btnid,nvalue)) return 0;
        icfg->srcbtnid=btnid;
        icfg->collection_phase=PS_ICFG_PHASE_HOLD1;
        icfg->value=nvalue;
      } break;

    case PS_ICFG_PHASE_HOLD1: {
        if (btnid!=icfg->srcbtnid) {
          return 0;
        } else if (!nvalue) {
          icfg->collection_phase=PS_ICFG_PHASE_CONFIRM;
          icfg->mismatch=0;
        } else if (nvalue!=icfg->value) {
          return ps_input_icfg_discard_collection(icfg);
        }
      } break;

    case PS_ICFG_PHASE_CONFIRM: {
        if (btnid!=icfg->srcbtnid) {
          if (!nvalue) return 0;
          if (ps_input_icfg_discard_collection(icfg)<0) return -1;
          icfg->mismatch=1;
          return 0;
        } else if (!nvalue) {
        } else if (nvalue==icfg->value) {
          icfg->collection_phase=PS_ICFG_PHASE_HOLD2;
        } else {
          return ps_input_icfg_discard_collection(icfg);
        }
      } break;

    case PS_ICFG_PHASE_HOLD2: {
        if (btnid!=icfg->srcbtnid) {
          return 0;
        } else if (!nvalue) {
          if (ps_input_icfg_accept_collection(icfg,btncfg)<0) return -1;
          return 1;
        } else if (nvalue!=icfg->value) {
          return ps_input_icfg_discard_collection(icfg);
        }
      } break;

  }
  
  return 0;
}

/* Finish, high-level convenience.
 */
 
int ps_input_icfg_finish(struct ps_input_icfg *icfg,int playerid) {
  if (!ps_input_icfg_is_ready(icfg)) return -1;
  
  struct ps_input_map *map=ps_input_icfg_generate_map(icfg,playerid);
  if (!map) return -1;

  if (ps_input_icfg_install_map(icfg,map)<0) {
    ps_input_map_del(map);
    return -1;
  }
  ps_input_map_del(map);

  struct ps_input_maptm *maptm=ps_input_icfg_generate_maptm(icfg);
  if (!maptm) return -1;

  if (ps_input_icfg_install_maptm(icfg,maptm)<0) {
    ps_input_maptm_del(maptm);
    return -1;
  }
  
  if (ps_input_force_device_assignment(icfg->device,playerid)<0) return -1;

  return 0;
}

/* Populate map object from icfg.
 */

static int ps_input_icfg_populate_map(struct ps_input_map *map,const struct ps_input_icfg *icfg) {
  const struct ps_input_icfg_map *src=icfg->mapv;
  int i=0; for (;i<icfg->mapc;i++,src++) {

    const struct ps_input_btncfg *btncfg=ps_input_premap_get(icfg->device->premap,src->srcbtnid);
    if (!btncfg) return -1;

    struct ps_input_map_fld *dst=ps_input_map_insert(map,-1,src->srcbtnid);
    if (!dst) return -1;

    dst->srcbtnid=src->srcbtnid;
    dst->dstbtnid=src->dstbtnid;
    dst->srclo=src->srclo;
    dst->srchi=src->srchi;
    dst->srcv=btncfg->value;
    dst->dstv=0;
    
  }
  return 0;
}

/* Generate map from finished icfg.
 */
 
struct ps_input_map *ps_input_icfg_generate_map(const struct ps_input_icfg *icfg,int playerid) {
  if (!ps_input_icfg_is_ready(icfg)) return 0;
  if (!icfg->device) return 0;

  struct ps_input_map *map=ps_input_map_new();
  if (!map) return 0;

  if (ps_input_icfg_populate_map(map,icfg)<0) {
    ps_input_map_del(map);
    return 0;
  }

  if (!ps_input_map_can_support_player(map)) {
    ps_log(INPUT,ERROR,"Produced invalid input map.");
    ps_input_map_del(map);
    return 0;
  }

  return map;
}

/* Install map in icfg's device.
 */

int ps_input_icfg_install_map(const struct ps_input_icfg *icfg,struct ps_input_map *map) {
  if (!icfg||!map) return -1;
  if (ps_input_device_set_map(icfg->device,map)<0) return -1;
  return 0;
}

/* Generate maptm from icfg's device.
 */
 
struct ps_input_maptm *ps_input_icfg_generate_maptm(const struct ps_input_icfg *icfg) {
  if (!icfg) return 0;
  return ps_input_maptm_generate_from_device(icfg->device);
}

/* Install maptm in global configuration.
 */
 
int ps_input_icfg_install_maptm(const struct ps_input_icfg *icfg,struct ps_input_maptm *maptm) {
  struct ps_input_config *config=ps_input_get_configuration();
  if (!config) return -1;

  ps_input_config_remove_similar_maptm(config,maptm);

  // HANDOFF
  if (ps_input_config_install_maptm(config,maptm)<0) {
    ps_input_maptm_del(maptm);
    return -1;
  }

  if (ps_input_config_save(0,config)<0) {
    return -1;
  }

  return 0;
}
