#include "ps.h"
#include "ps_machid.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include "input/ps_input_button.h"

/* Global.
 */

static struct ps_input_provider *ps_machid_provider=0;

struct ps_input_device_machid {
  struct ps_input_device hdr;
  int machid_devid;
};
#define DEVICE ((struct ps_input_device_machid*)device)

/* Get PS device by MacHID ID.
 */

static struct ps_input_device *ps_machid_get_ps_device(int devid) {
  if (!ps_machid_provider) return 0;
  int i=ps_machid_provider->devc; for (;i-->0;) {
    struct ps_input_device *device=ps_machid_provider->devv[i];
    if (DEVICE->machid_devid==devid) return device;
  }
  return 0;
}

/* Delegate callbacks.
 */

static int ps_machid_cb_connect(int devid) {
  ps_log(MACHID,INFO,"%s %d",__func__,devid);
  
  struct ps_input_device *device=ps_input_device_new(sizeof(struct ps_input_device_machid));
  if (!device) return -1;

  if (ps_input_provider_install_device(ps_machid_provider,device)<0) {
    ps_input_device_del(device);
    return -1;
  }
  ps_input_device_del(device);

  ps_input_device_set_name(device,ps_machid_dev_get_product_name(devid),-1);
  device->vendorid=ps_machid_dev_get_vendor_id(devid);
  device->deviceid=ps_machid_dev_get_product_id(devid);
  DEVICE->machid_devid=devid;

  if (ps_input_event_connect(device)<0) return -1;
  
  return 0;
}

static int ps_machid_cb_disconnect(int devid) {
  ps_log(MACHID,INFO,"%s %d",__func__,devid);
  struct ps_input_device *device=ps_machid_get_ps_device(devid);
  if (!device) return 0;
  int err=ps_input_event_disconnect(device);
  ps_input_provider_uninstall_device(ps_machid_provider,device);
  return err;
}

static int ps_machid_cb_button(int devid,int btnid,int value) {
  if (btnid!=16&&btnid!=17&&btnid!=18&&btnid!=19&&btnid!=32&&btnid!=33&&btnid!=34&&btnid!=35)
  ps_log(MACHID,TRACE,"%s %d.%d=%d",__func__,devid,btnid,value);
  struct ps_input_device *device=ps_machid_get_ps_device(devid);
  if (!device) return 0;
  return ps_input_event_button(device,btnid,value);
}

/* Provider callbacks.
 */

static int ps_machid_cb_update(struct ps_input_provider *provider) {
  return ps_machid_update();
}

static int ps_machid_cb_report_buttons(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  if (!device||!cb) return -1;
  int btnc=ps_machid_dev_count_buttons(DEVICE->machid_devid);
  int btni=0; for (;btni<btnc;btni++) {
    struct ps_input_btncfg btncfg={0};
    int usage=0;
    if (ps_machid_dev_get_button_info(&btncfg.srcbtnid,&usage,&btncfg.lo,&btncfg.hi,&btncfg.value,DEVICE->machid_devid,btni)>=0) {
      int err=cb(device,&btncfg,userdata);
      if (err<0) return err;
    }
  }
  return 0;
}

/* PS-aware delegate.
 */

const struct ps_machid_delegate ps_machid_default_delegate={
  .test_device=0, // Default behavior is acceptable.
  .connect=ps_machid_cb_connect,
  .disconnect=ps_machid_cb_disconnect,
  .button=ps_machid_cb_button,
};

/* Create input provider.
 */
 
struct ps_input_provider *ps_machid_new_provider() {
  if (!ps_machid_provider) {
    if (!(ps_machid_provider=ps_input_provider_new(0))) return 0;
    ps_machid_provider->providerid=PS_INPUT_PROVIDER_machid;
    ps_machid_provider->update=ps_machid_cb_update;
    ps_machid_provider->report_buttons=ps_machid_cb_report_buttons;
  }
  return ps_machid_provider;
}

/* Dispose provider.
 */

void ps_machid_destroy_global_provider() {
  ps_input_provider_del(ps_machid_provider);
  ps_machid_provider=0;
}
