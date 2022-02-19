#include "ps_input_internal.h"
#include "ps_input_config.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"
#include "gui/ps_gui.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/poll.h>

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

  ps_gui_del(ps_input.gui);

  if (ps_input.providerv) {
    while (ps_input.providerc>0) ps_input_uninstall_provider(ps_input.providerv[0]);
    free(ps_input.providerv);
  }

  ps_input_config_del(ps_input.config);

  if (ps_input.watchv) {
    free(ps_input.watchv);
  }
  
  memset(&ps_input,0,sizeof(struct ps_input));
}

/* Update.
 */

int ps_input_update() {
  if (!ps_input.init) return 0;

  if (ps_input.suppress_player_buttons>0) {
    ps_input.suppress_player_buttons--;
  }
  
  // XXX A quick n dirty hack to quit on ENTER from stdin.
  // This is worth keeping, but should be implemented as an input provider.
  struct pollfd pollfd={.fd=STDIN_FILENO,.events=POLLIN};
  if (poll(&pollfd,1,0)>0) {
    if (pollfd.revents&POLLIN) {
      char src[32];
      int srcc=read(STDIN_FILENO,src,sizeof(src));
      if ((srcc==1)&&(src[0]==0x0a)) {
        fprintf(stderr,"ENTER on stdin: Quitting.\n");
        ps_input.termination_requested=1;
        return 0;
      }
    }
  }

  int i=0; for (;i<ps_input.providerc;i++) {
    struct ps_input_provider *provider=ps_input.providerv[i];
    if (provider->update) {
      if (provider->update(provider)<0) return -1;
    }
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

/* GUI.
 */
 
int ps_input_set_gui(struct ps_gui *gui) {
  if (!ps_input.init) return -1;
  if (gui==ps_input.gui) return 0;
  if (ps_gui_ref(gui)<0) return -1;
  ps_gui_del(ps_input.gui);
  ps_input.gui=gui;
  return 0;
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

/* Get configuration.
 */
 
struct ps_input_config *ps_input_get_configuration() {
  return ps_input.config;
}

/* Report player buttons to the public.
 */
 
uint16_t ps_get_player_buttons(int plrid) {
  if ((plrid<0)||(plrid>PS_PLAYER_LIMIT)) return 0;
  if (ps_input.suppress_player_buttons) {
    return ps_input.plrbtnv[plrid]&PS_PLRBTN_ALWAYS_AVAILABLE;
  }
  return ps_input.plrbtnv[plrid];
}

/* Suppress player buttons.
 */
 
int ps_input_suppress_player_actions(int duration) {
  if (duration<ps_input.suppress_player_buttons) return 0;
  ps_input.suppress_player_buttons=duration;
  return 0;
}

/* Call function for each connected device.
 */

static int ps_input_identify_connected_devices(
  int (*cb_connect)(struct ps_input_device *device,void *userdata),
  void *userdata
) {
  int pi=ps_input.providerc; while (pi-->0) {
    struct ps_input_provider *provider=ps_input.providerv[pi];
    int di=provider->devc; while (di-->0) {
      struct ps_input_device *device=provider->devv[di];
      int err=cb_connect(device,userdata);
      if (err<0) return err;
    }
  }
  return 0;
}

/* Watch list primitives.
 */

static int ps_input_watch_require() {
  if (ps_input.watchc<ps_input.watcha) return 0;
  int na=ps_input.watcha+8;
  if (na>INT_MAX/sizeof(struct ps_input_watch)) return -1;
  void *nv=realloc(ps_input.watchv,sizeof(struct ps_input_watch)*na);
  if (!nv) return -1;
  ps_input.watchv=nv;
  ps_input.watcha=na;
  return 0;
}

static int ps_input_watch_select_id(int *watchid) {
  if (ps_input.watchc<1) {
    *watchid=1;
    return 0;
  }
  *watchid=ps_input.watchv[ps_input.watchc-1].watchid;
  if (*watchid<INT_MAX) {
    (*watchid)++;
    return ps_input.watchc;
  }
  int p=0;
  *watchid=1;
  while (1) {
    if (*watchid!=ps_input.watchv[p].watchid) return p;
    (*watchid)++;
    p++;
  }
}

static int ps_input_watch_search(int watchid) {
  int lo=0,hi=ps_input.watchc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (watchid<ps_input.watchv[ck].watchid) hi=ck;
    else if (watchid>ps_input.watchv[ck].watchid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Register or unregister watchers.
 */
 
int ps_input_watch_devices(
  int (*cb_connect)(struct ps_input_device *device,void *userdata),
  int (*cb_disconnect)(struct ps_input_device *device,void *userdata),
  void *userdata
) {

  if (ps_input_watch_require()<0) return -1;
  int watchid,p;
  p=ps_input_watch_select_id(&watchid);
  if ((p<0)||(watchid<1)) return -1;

  struct ps_input_watch *watch=ps_input.watchv+p;
  memmove(watch+1,watch,sizeof(struct ps_input_watch)*(ps_input.watchc-p));
  ps_input.watchc++;
  memset(watch,0,sizeof(struct ps_input_watch));
  watch->watchid=watchid;
  watch->cb_connect=cb_connect;
  watch->cb_disconnect=cb_disconnect;
  watch->userdata=userdata;

  if (cb_connect) {
    int err=ps_input_identify_connected_devices(cb_connect,userdata);
    if (err<0) {
      ps_input_unwatch_devices(watchid);
      return err;
    }
  }

  return watchid;
}

int ps_input_unwatch_devices(int watchid) {
  if (watchid<1) return -1;
  int p=ps_input_watch_search(watchid);
  if (p<0) return -1;
  ps_input.watchc--;
  memmove(ps_input.watchv+p,ps_input.watchv+p+1,sizeof(struct ps_input_watch)*(ps_input.watchc-p));
  return 0;
}
