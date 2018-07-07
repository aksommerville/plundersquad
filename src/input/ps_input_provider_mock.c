#include "ps.h"
#include "ps_input.h"
#include "ps_input_provider.h"
#include "ps_input_button.h"
#include "ps_input_device.h"

/* Object definition.
 */
 
struct ps_input_device_mock {
  struct ps_input_device hdr;
  uint16_t state;
};

struct ps_input_provider_mock {
  struct ps_input_provider hdr;
};

#define DEVICE ((struct ps_input_device_mock*)device)
#define PROVIDER ((struct ps_input_provider_mock*)provider)

/* Update.
 */
 
static int ps_input_provider_mock_update(struct ps_input_provider *provider) {
  return 0;
}

/* Report buttons.
 */
 
static int ps_input_provider_mock_report_buttons(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  #define BTN(btnid) { \
    struct ps_input_btncfg btncfg={ \
      .srcbtnid=btnid, \
      .lo=0, \
      .hi=1, \
      .value=(DEVICE->state&btnid)?1:0, \
      .default_usage=btnid, \
    }; \
    int err=cb(device,&btncfg,userdata); \
    if (err) return err; \
  }
  BTN(PS_PLRBTN_UP)
  BTN(PS_PLRBTN_DOWN)
  BTN(PS_PLRBTN_LEFT)
  BTN(PS_PLRBTN_RIGHT)
  BTN(PS_PLRBTN_A)
  BTN(PS_PLRBTN_B)
  BTN(PS_PLRBTN_START)
  #undef BTN
  return 0;
}

/* Public ctor.
 */

struct ps_input_provider *ps_input_provider_mock_new() {
  struct ps_input_provider *provider=ps_input_provider_new(sizeof(struct ps_input_provider_mock));
  if (!provider) return 0;
  
  provider->providerid=PS_INPUT_PROVIDER_mock;
  provider->update=ps_input_provider_mock_update;
  provider->report_buttons=ps_input_provider_mock_report_buttons;
  
  return provider;
}

/* Search devices.
 */
 
static int ps_input_provider_mock_search(const struct ps_input_provider *provider,const char *name) {
  if (!name) return 0;
  int namec=0; while (name[namec]) namec++;
  int i=provider->devc; while (i-->0) {
    struct ps_input_device *device=provider->devv[i];
    if (namec!=device->namec) continue;
    if (memcmp(name,device->name,namec)) continue;
    return i;
  }
  return -1;
}

/* Add device.
 */
 
int ps_input_provider_mock_add_device(struct ps_input_provider *provider,const char *name) {
  if (!provider||(provider->providerid!=PS_INPUT_PROVIDER_mock)) return -1;
  int p=ps_input_provider_mock_search(provider,name);
  if (p>=0) {
    ps_log(INPUT,ERROR,"Duplicate mock device '%s'",name);
    return -1;
  }
  struct ps_input_device *device=ps_input_device_new(sizeof(struct ps_input_device_mock));
  if (!device) return -1;
  device->providerid=PS_INPUT_PROVIDER_mock;
  device->report_buttons=ps_input_provider_mock_report_buttons;
  if (ps_input_provider_install_device(provider,device)<0) {
    ps_input_device_del(device);
    return -1;
  }
  ps_input_device_del(device);
  if (ps_input_device_set_name(device,name,-1)<0) return -1;
  if (ps_input_event_connect(device)<0) return -1;
  return 0;
}

/* Remove device.
 */
 
int ps_input_provider_mock_remove_device(struct ps_input_provider *provider,const char *name) {
  if (!provider||(provider->providerid!=PS_INPUT_PROVIDER_mock)) return -1;
  int p=ps_input_provider_mock_search(provider,name);
  if (p<0) return 0;
  struct ps_input_device *device=provider->devv[p];
  if (ps_input_event_disconnect(device)<0) return -1;
  if (ps_input_provider_uninstall_device(provider,provider->devv[p])<0) return -1;
  return 0;
}

/* Set button.
 */
 
int ps_input_provider_mock_set_button(struct ps_input_provider *provider,const char *name,int btnid,int value) {
  if (!provider||(provider->providerid!=PS_INPUT_PROVIDER_mock)) return -1;
  int p=ps_input_provider_mock_search(provider,name);
  if (p<0) return -1;
  struct ps_input_device *device=provider->devv[p];
  if (value) {
    DEVICE->state|=btnid;
  } else {
    DEVICE->state&=~btnid;
  }
  if (ps_input_event_button(device,btnid,value)<0) return -1;
  return 0;
}
