#include "ps_input_internal.h"
#include "ps_input_config.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"

struct ps_input ps_input={0};

/* Init.
 */

int ps_input_init() {

  if (ps_input.init) return -1;
  memset(&ps_input,0,sizeof(struct ps_input));
  ps_input.init=1;

  if (!(ps_input.config=ps_input_config_new())) {
    ps_input_quit();
    return -1;
  }
  ps_input.preconfig=1;
  ps_input.playerc=PS_PLAYER_LIMIT;

  return 0;
}

/* Quit.
 */

void ps_input_quit() {

  if (ps_input.providerv) {
    while (ps_input.providerc>0) ps_input_uninstall_provider(ps_input.providerv[0]);
    free(ps_input.providerv);
  }

  ps_input_config_del(ps_input.config);
  
  memset(&ps_input,0,sizeof(struct ps_input));
}

/* Update.
 */

int ps_input_update() {
  if (!ps_input.init) return 0;

  int i=0; for (;i<ps_input.providerc;i++) {
    struct ps_input_provider *provider=ps_input.providerv[i];
    if (provider->update) {
      if (provider->update(provider)<0) return -1;
    }
  }

  if (ps_input.mapped_devices_changed) {
    if (ps_input_reassign_devices()<0) return -1;
    ps_input.mapped_devices_changed=0;
  }
  
  return 0;
}

/* Termination flag.
 */
 
int ps_input_request_termination() {
  ps_input.termination_requested=1;
  return 0;
}

int ps_input_termination_requested() {
  int result=ps_input.termination_requested;
  ps_input.termination_requested=0;
  return result;
}

/* Provider list public accesors.
 */
 
int ps_input_install_provider(struct ps_input_provider *provider) {
  if (!provider) return -1;
  if (!ps_input.init) return -1;

  int i=ps_input.providerc; while (i-->0) {
    struct ps_input_provider *existing=ps_input.providerv[i];
    if (existing==provider) return 0; // Already installed.
    if (existing->providerid==provider->providerid) return -1; // ID conflict.
  }

  if (ps_input.providerc>=ps_input.providera) {
    int na=ps_input.providera+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ps_input.providerv,sizeof(void*)*na);
    if (!nv) return -1;
    ps_input.providerv=nv;
    ps_input.providera=na;
  }

  if (ps_input_provider_ref(provider)<0) return -1;
  ps_input.providerv[ps_input.providerc]=provider;
  ps_input.providerc++;
  
  return 0;
}

int ps_input_uninstall_provider(struct ps_input_provider *provider) {
  if (!provider) return -1;
  int i=0; for (;i<ps_input.providerc;i++) {
    if (ps_input.providerv[i]==provider) {
      ps_input.providerc--;
      memmove(ps_input.providerv+i,ps_input.providerv+i+1,sizeof(void*)*(ps_input.providerc-i));
      ps_input_provider_del(provider);
      return 0;
    }
  }
  return -1;
}

int ps_input_count_providers() {
  return ps_input.providerc;
}

struct ps_input_provider *ps_input_get_provider_by_index(int index) {
  if ((index<0)||(index>=ps_input.providerc)) return 0;
  return ps_input.providerv[index];
}

struct ps_input_provider *ps_input_get_provider_by_id(int providerid) {
  int i=ps_input.providerc; for (;i-->0;) {
    if (ps_input.providerv[i]->providerid==providerid) return ps_input.providerv[i];
  }
  return 0;
}

/* Reconnect unmapped devices, after loading configuration.
 */

static int ps_input_reconnect_unmapped_devices() {
  int pi; for (pi=0;pi<ps_input.providerc;pi++) {
    struct ps_input_provider *provider=ps_input.providerv[pi];
    int di; for (di=0;di<provider->devc;di++) {
      struct ps_input_device *device=provider->devv[di];
      if (device->map) continue;
      if (ps_input_event_connect(device)<0) return -1;
    }
  }
  return 0;
}

/* Load configuration.
 */
 
int ps_input_load_configuration(const char *path) {
  if (!ps_input.init) return -1;
  if (ps_input_config_load(ps_input.config,path)<0) {
    return -1;
  }
  ps_input.preconfig=0;
  if (ps_input_reconnect_unmapped_devices()<0) return -1;
  return 0;
}

/* Report player buttons to the public.
 */
 
uint16_t ps_get_player_buttons(int plrid) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return 0;
  return ps_input.plrbtnv[plrid];
}
