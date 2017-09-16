#include "ps_input_internal.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"
#include "ps_input_map.h"

/* Set player count, public.
 */
 
int ps_input_set_player_count(int playerc) {
  if ((playerc<0)||(playerc>PS_PLAYER_LIMIT)) return -1;
  ps_input.playerc=playerc;
  return ps_input_reassign_devices();
}

/* Input reassignment context.
 */

struct ps_input_reassign_devices {
  struct ps_input_device **devv;
  int devc,deva;
  int devc_per_player[1+PS_PLAYER_LIMIT];
};

static void ps_input_reassign_devices_cleanup(struct ps_input_reassign_devices *ctx) {
  if (ctx->devv) free(ctx->devv);
}

static int ps_input_reassign_devices_add_device(struct ps_input_reassign_devices *ctx,struct ps_input_device *dev) {
  int i=ctx->devc; for (;i-->0;) if (ctx->devv[i]==dev) return 0;
  if (ctx->devc>=ctx->deva) {
    int na=ctx->deva+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->devv,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->devv=nv;
    ctx->deva=na;
  }
  ctx->devv[ctx->devc++]=dev;
  return 0;
}

static struct ps_input_device *ps_input_reassign_get_any_device_for_player(const struct ps_input_reassign_devices *ctx,int plrid) {
  int i=ctx->devc; for (;i-->0;) if (ctx->devv[i]->map->plrid==plrid) return ctx->devv[i];
  return 0;
}

/* Gather all assignable devices into context.
 */

static int ps_input_gather_assignable_devices(struct ps_input_reassign_devices *ctx) {
  int pi=0; for (;pi<ps_input.providerc;pi++) {
    struct ps_input_provider *provider=ps_input.providerv[pi];
    int di=0; for (;di<provider->devc;di++) {
      struct ps_input_device *device=provider->devv[di];
      if (!device->map) continue;
      if (!ps_input_map_can_support_player(device->map)) continue;
      if (ps_input_reassign_devices_add_device(ctx,device)<0) return -1;
    }
  }
  return 0;
}

/* Set to player zero, any device whose playerid is out of range.
 * Also in this pass, count assignments per player.
 */

static int ps_input_drop_invalid_player_assignments(struct ps_input_reassign_devices *ctx) {
  memset(ctx->devc_per_player,0,sizeof(ctx->devc_per_player));
  int i=ctx->devc; while (i-->0) {
    struct ps_input_device *device=ctx->devv[i];
    if ((device->map->plrid<0)||(device->map->plrid>ps_input.playerc)) {
      device->map->plrid=0;
    }
    ctx->devc_per_player[device->map->plrid]++;
  }
  return 0;
}

/* If there are multiple devices on any valid player, check whether we might need them for another player.
 * If so, reassign them to player zero.
 * Use and update ctx->devc_per_player.
 */

static int ps_input_reassign_count_needy_players(const struct ps_input_reassign_devices *ctx) {
  int c=0,i; for (i=1;i<=ps_input.playerc;i++) {
    if (!ctx->devc_per_player[i]) c++;
  }
  return c;
}

static int ps_input_drop_duplicate_assignments_if_needed(struct ps_input_reassign_devices *ctx) {
  int needy_playerc=ps_input_reassign_count_needy_players(ctx);
  if (needy_playerc<=ctx->devc_per_player[0]) return 0; // We can satisfy the needy ones from what's orphaned already.
  int plrid=1; for (;plrid<=ps_input.playerc;plrid++) {
    while (ctx->devc_per_player[plrid]>1) {
      struct ps_input_device *device=ps_input_reassign_get_any_device_for_player(ctx,plrid);
      if (!device) return -1;
      device->map->plrid=0;
      ctx->devc_per_player[plrid]--;
      ctx->devc_per_player[0]++;
      if (!--needy_playerc) return 0; // Got enough, OK, let's stop.
    }
  }
  return 0;
}

/* Any devices still unassigned, pick the most appropriate player for them.
 */

static int ps_input_select_player_candidate_with_joystick(struct ps_input_reassign_devices *ctx,int *candidatev,int candidatec) {
  int i; for (i=0;i<candidatec;i++) {
    int plrid=candidatev[i];
    int di; for (di=0;di<ctx->devc;di++) {
      struct ps_input_device *device=ctx->devv[di];
      if (device->map->plrid!=plrid) continue;
      if (ps_input_device_is_joystick(device)) return plrid;
    }
  }
  return -1;
}

static int ps_input_select_player_candidate_with_keyboard(struct ps_input_reassign_devices *ctx,int *candidatev,int candidatec) {
  int i; for (i=0;i<candidatec;i++) {
    int plrid=candidatev[i];
    int di; for (di=0;di<ctx->devc;di++) {
      struct ps_input_device *device=ctx->devv[di];
      if (device->map->plrid!=plrid) continue;
      if (ps_input_device_is_keyboard(device)) return plrid;
    }
  }
  return -1;
}

static int ps_input_select_plrid_for_device(struct ps_input_reassign_devices *ctx,struct ps_input_device *device) {
  // Is any player without a device?
  int devcmin=INT_MAX;
  int candidatev[PS_PLAYER_LIMIT]; // plrid with devc==devcmin
  int candidatec=0;
  int plrid=1; for (;plrid<=ps_input.playerc;plrid++) {
    if (!ctx->devc_per_player[plrid]) return plrid;
    if (ctx->devc_per_player[plrid]<devcmin) {
      devcmin=ctx->devc_per_player[plrid];
      candidatev[0]=plrid;
      candidatec=1;
    } else if (ctx->devc_per_player[plrid]==devcmin) {
      candidatev[candidatec++]=plrid;
    }
  }
  // If we are a keyboard, look for a candidate that already has a joystick.
  if (ps_input_device_is_keyboard(device)) {
    int plrid=ps_input_select_player_candidate_with_joystick(ctx,candidatev,candidatec);
    if (plrid>0) return plrid;
  // If we are a joystick, look for a candidate that already has a keyboard.
  } else {
    int plrid=ps_input_select_player_candidate_with_keyboard(ctx,candidatev,candidatec);
    if (plrid>0) return plrid;
  }
  // Use the first candidate.
  return candidatev[0];
}

static int ps_input_assign_devices_to_players(struct ps_input_reassign_devices *ctx) {
  int devi=0; for (;devi<ctx->devc;devi++) {
    struct ps_input_device *device=ctx->devv[devi];
    if (device->map->plrid) continue;
    int plrid=ps_input_select_plrid_for_device(ctx,device);
    if (plrid<0) return -1;
    ctx->devc_per_player[0]--;
    ctx->devc_per_player[plrid]++;
    device->map->plrid=plrid;
  }
  return 0;
}

/* Reassign devices to players.
 * The basic rules:
 *  - Devices without a map are ignored.
 *  - Devices whose map doesn't support players are ignored.
 *  - If a player has devices already, keep at least one of them.
 *  - Assign only to valid players (ie plrid<=playerc).
 *  - It's OK to assign to player zero, but only if playerc is zero.
 *  - Prefer lower players. If there's not enough devices, player 1 gets one but player 7 might not.
 *  - Issue a warning if some players are left unassigned.
 *  - It's OK to assign more than one device to a player, if there are more devices than players.
 *  - If double-assigning, try to pair keyboard with joystick instead of joystick+joystick.
 */
 
int ps_input_reassign_devices() {
  struct ps_input_reassign_devices ctx={0};

  if (ps_input_gather_assignable_devices(&ctx)<0) goto _error_;
  if (ps_input_drop_invalid_player_assignments(&ctx)<0) goto _error_;
  if (ps_input_drop_duplicate_assignments_if_needed(&ctx)<0) goto _error_;
  if (ps_input_assign_devices_to_players(&ctx)<0) goto _error_;

  ps_input_reassign_devices_cleanup(&ctx);
  return 0;
 _error_:
  ps_input_reassign_devices_cleanup(&ctx);
  return -1;
}
