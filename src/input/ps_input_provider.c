#include "ps.h"
#include "ps_input_provider.h"
#include "ps_input_device.h"

/* New.
 */

struct ps_input_provider *ps_input_provider_new(int size) {
  if (!size) size=sizeof(struct ps_input_provider);
  else if (size<(int)sizeof(struct ps_input_provider)) return 0;

  struct ps_input_provider *provider=calloc(1,size);
  if (!provider) return 0;

  provider->refc=1;

  return provider;
}

/* Delete.
 */
 
void ps_input_provider_del(struct ps_input_provider *provider) {
  if (!provider) return;
  if (provider->refc-->1) return;

  if (provider->devv) {
    while (provider->devc-->0) ps_input_device_del(provider->devv[provider->devc]);
    free(provider->devv);
  }

  free(provider);
}

/* Retain.
 */
 
int ps_input_provider_ref(struct ps_input_provider *provider) {
  if (!provider) return -1;
  if (provider->refc<1) return -1;
  if (provider->refc==INT_MAX) return -1;
  provider->refc++;
  return 0;
}

/* Search for device.
 */
 
int ps_input_provider_device_search(const struct ps_input_provider *provider,int devid) {
  if (!provider) return -1;
  int lo=0,hi=provider->devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<provider->devv[ck]->devid) hi=ck;
    else if (devid>provider->devv[ck]->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct ps_input_device *ps_input_provider_get_device_by_devid(const struct ps_input_provider *provider,int devid) {
  int p=ps_input_provider_device_search(provider,devid);
  if (p<0) return 0;
  return provider->devv[p];
}

/* Install device.
 */
 
int ps_input_provider_install_device(struct ps_input_provider *provider,struct ps_input_device *device) {
  if (!provider||!device) return -1;

  if (device->devid<1) device->devid=ps_input_provider_unused_devid(provider);
  
  int p=ps_input_provider_device_search(provider,device->devid);
  if (p>=0) return -1;
  p=-p-1;

  if (provider->devc>=provider->deva) {
    int na=provider->deva+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(provider->devv,sizeof(void*)*na);
    if (!nv) return -1;
    provider->devv=nv;
    provider->deva=na;
  }

  if (ps_input_device_ref(device)<0) return -1;
  memmove(provider->devv+p+1,provider->devv+p,sizeof(void*)*(provider->devc-p));
  provider->devc++;
  provider->devv[p]=device;
  
  device->providerid=provider->providerid;
  if (!device->report_buttons) {
    device->report_buttons=provider->report_buttons;
  }

  return 0;
}

/* Uninstall device.
 */
 
int ps_input_provider_uninstall_device(struct ps_input_provider *provider,struct ps_input_device *device) {
  if (!provider) return -1;
  if (!device) return -1;
  int p=ps_input_provider_device_search(provider,device->devid);
  if (p<0) return -1;
  if (provider->devv[p]!=device) return -1;
  provider->devc--;
  memmove(provider->devv+p,provider->devv+p+1,sizeof(void*)*(provider->devc-p));
  ps_input_device_del(device);
  return 0;
}

/* Get unused devid.
 */
 
int ps_input_provider_unused_devid(const struct ps_input_provider *provider) {
  if (!provider) return 0;
  if (provider->devc<1) return 1;
  struct ps_input_device *final=provider->devv[provider->devc-1];
  if (final->devid<INT_MAX) return final->devid+1;
  int devid=1,p=0;
  while (1) {
    if (p>=provider->devc) return devid;
    if (devid!=provider->devv[p]->devid) return devid;
    p++;
    devid++;
  }
}
