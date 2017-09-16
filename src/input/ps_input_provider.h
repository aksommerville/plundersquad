/* ps_input_provider.h
 * Interface between manager and driver.
 */

#ifndef PS_INPUT_PROVIDER_H
#define PS_INPUT_PROVIDER_H

struct ps_input_device;
struct ps_input_btncfg;

struct ps_input_provider {
  int providerid;
  int refc;

  struct ps_input_device **devv;
  int devc,deva;

  int (*btnid_repr)(char *dst,int dsta,int btnid);
  int (*btnid_eval)(int *btnid,const char *src,int srcc);

  int (*update)(struct ps_input_provider *provider);

  // Trigger your callback for each button on this device, until you return nonzero.
  int (*report_buttons)(
    struct ps_input_device *device,
    void *userdata,
    int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
  );
  
};

struct ps_input_provider *ps_input_provider_new(int size);
void ps_input_provider_del(struct ps_input_provider *provider);
int ps_input_provider_ref(struct ps_input_provider *provider);

int ps_input_provider_device_search(const struct ps_input_provider *provider,int devid);
struct ps_input_device *ps_input_provider_get_device_by_devid(const struct ps_input_provider *provider,int devid);
int ps_input_provider_install_device(struct ps_input_provider *provider,struct ps_input_device *device);
int ps_input_provider_uninstall_device(struct ps_input_provider *provider,struct ps_input_device *device);

int ps_input_provider_unused_devid(const struct ps_input_provider *provider);

#endif
