#include "ps.h"
#include "ps_evdev.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include <linux/input.h>

/* Input provider type.
 */

struct ps_input_provider_evdev {
  struct ps_input_provider hdr;
};

#define PROVIDER ((struct ps_input_provider_evdev*)provider)

static struct ps_input_provider *ps_input_provider_evdev=0;

/* Update adapter.
 */

static int ps_evdev_default_update(struct ps_input_provider *provider) {
  if (ps_evdev_update()<0) return -1;
  return 0;
}

/* Capability report adapter.
 */

struct ps_evdev_default_report_ctx {
  void *userdata;
  struct ps_input_device *device;
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata);
};

static int ps_evdev_default_report_buttons_cb(int devid,int type,int code,int lo,int hi,int value,void *userdata) {
  struct ps_evdev_default_report_ctx *ctx=userdata;
  struct ps_input_btncfg btncfg={
    .srcbtnid=(type<<16)|code,
    .lo=lo,
    .hi=hi,
    .value=value,
    .default_usage=ps_evdev_default_usage_for_event(type,code),
  };
  return ctx->cb(ctx->device,&btncfg,ctx->userdata);
}

int ps_evdev_default_report_buttons(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  struct ps_evdev_default_report_ctx ctx={
    .userdata=userdata,
    .device=device,
    .cb=cb,
  };
  if (ps_evdev_report_capabilities(device->devid,ps_evdev_default_report_buttons_cb,&ctx)<0) return -1;
  return 0;
}

/* Create provider.
 */
 
static int ps_evdev_init_provider() {

  struct ps_input_provider *provider=ps_input_provider_new(sizeof(struct ps_input_provider_evdev));
  if (!provider) return -1;

  provider->providerid=PS_INPUT_PROVIDER_evdev;
  provider->update=ps_evdev_default_update;
  provider->report_buttons=ps_evdev_default_report_buttons;

  if (ps_input_install_provider(provider)<0) {
    ps_input_provider_del(provider);
    return -1;
  }

  ps_input_provider_evdev=provider;
  return 0;
}

/* Callbacks.
 */

static int ps_evdev_default_cb_connect(int devid) {
  ps_log(EVDEV,TRACE,"%s %d",__func__,devid);

  if (!ps_input_provider_evdev) return -1;
  struct ps_input_device *device=ps_input_provider_get_device_by_devid(ps_input_provider_evdev,devid);
  if (device) {
    ps_log(INPUT,ERROR,"Evdev device ID %d already in use.",devid);
    return 0;
  }

  device=ps_input_device_new(sizeof(struct ps_input_device));
  if (!device) return -1;

  device->providerid=PS_INPUT_PROVIDER_evdev;
  device->devid=devid;
  device->report_buttons=ps_input_provider_evdev->report_buttons;

  const char *name=ps_evdev_get_name(devid);
  if (ps_input_device_set_name(device,name,-1)<0) return -1;

  int vendorid,deviceid;
  if (ps_evdev_get_id(0,&vendorid,&deviceid,0,devid)>=0) {
    device->vendorid=vendorid;
    device->deviceid=deviceid;
  }

  if (ps_input_provider_install_device(ps_input_provider_evdev,device)<0) {
    ps_input_device_del(device);
    return -1;
  }
  ps_input_device_del(device);

  if (ps_input_event_connect(device)<0) {
    return -1;
  }

  return 0;
}

static int ps_evdev_default_cb_disconnect(int devid,int reason) {
  ps_log(EVDEV,TRACE,"%s %d %d",__func__,devid,reason);

  struct ps_input_device *device=ps_input_provider_get_device_by_devid(ps_input_provider_evdev,devid);
  if (device) {
    if (ps_input_event_disconnect(device)<0) return -1;
    ps_input_provider_uninstall_device(ps_input_provider_evdev,device);
  }
  
  return 0;
}

static int ps_evdev_default_cb_event(int devid,int type,int code,int value) {

  if (type==EV_MSC) return 0;

  ps_log(EVDEV,TRACE,"%s %d.%d:%d=%d",__func__,devid,type,code,value);

  /* EV_KEY will arrive with a value of 2 if it is an automatic key repeat.
   * That's cool and all, but the range still reports as 0..1, which it should.
   * For our purposes I think it's OK to discard the key-repeat events.
   */
  if ((type==EV_KEY)&&(value==2)) return 0;

  struct ps_input_device *device=ps_input_provider_get_device_by_devid(ps_input_provider_evdev,devid);
  if (device) {
    if (ps_input_event_button(device,(type<<16)|code,value)<0) return -1;
  }
  
  return 0;
}

/* Init with defaults.
 */
 
int ps_evdev_init_default() {

  if (ps_input_provider_evdev) return -1;
  if (ps_evdev_init_provider()<0) return -1;

  if (ps_evdev_init(0,ps_evdev_default_cb_connect,ps_evdev_default_cb_disconnect,ps_evdev_default_cb_event)<0) {
    ps_input_uninstall_provider(ps_input_provider_evdev);
    ps_input_provider_del(ps_input_provider_evdev);
    ps_input_provider_evdev=0;
    return -1;
  }

  return 0;
}
