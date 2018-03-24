#include "ps.h"
#include "ps_input_device.h"
#include "ps_input_map.h"
#include "ps_input_button.h"
#include "ps_input_premap.h"
#include "ps_input_report_reader.h"

/* Clean up button watcher.
 */

static void ps_input_btnwatch_cleanup(struct ps_input_btnwatch *watch) {
  if (!watch) return;
  if (watch->userdata_del) watch->userdata_del(watch);
  memset(watch,0,sizeof(struct ps_input_btnwatch));
}

/* Object lifecycle.
 */

struct ps_input_device *ps_input_device_new(int size) {
  if (!size) size=sizeof(struct ps_input_device);
  else if (size<(int)sizeof(struct ps_input_device)) return 0;

  struct ps_input_device *device=calloc(1,size);
  if (!device) return 0;

  device->refc=1;
  device->btnwatchid_next=1;

  return device;
}

void ps_input_device_del(struct ps_input_device *device) {
  if (!device) return;
  if (device->refc-->1) return;

  if (device->name) free(device->name);
  if (device->map) ps_input_map_del(device->map);
  if (device->premap) ps_input_premap_del(device->premap);

  if (device->btnwatchv) {
    while (device->btnwatchc-->0) {
      ps_input_btnwatch_cleanup(device->btnwatchv+device->btnwatchc);
    }
    free(device->btnwatchv);
  }

  ps_input_report_reader_del(device->report_reader);

  free(device);
}

int ps_input_device_ref(struct ps_input_device *device) {
  if (!device) return -1;
  if (device->refc<1) return -1;
  if (device->refc==INT_MAX) return -1;
  device->refc++;
  return 0;
}

/* Trivial accessors.
 */

int ps_input_device_set_name(struct ps_input_device *device,const char *src,int srcc) {
  if (!device) return -1;
  
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>=1024) srcc=1023;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }

  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=srcc;
  return 0;
}

int ps_input_device_set_name_utf16lez(struct ps_input_device *device,const char *src) {
  // We don't actually decode UTF-16 because who cares.
  char dst[256];
  int dstc=0,srcp=0;
  if (src) {
    while (1) {
      if (dstc>=sizeof(dst)) break;
      if (!src[srcp]&&!src[srcp+1]) break;
      if (src[srcp+1]||(src[srcp]<0x20)||(src[srcp]>0x7e)) {
        dst[dstc++]='?';
      } else {
        dst[dstc++]=src[srcp];
      }
      srcp+=2;
    }
  }
  return ps_input_device_set_name(device,dst,dstc);
}

int ps_input_device_set_map(struct ps_input_device *device,struct ps_input_map *map) {
  if (!device) return -1;
  if (device->map==map) return 0;
  if (map&&(ps_input_map_ref(map)<0)) return -1;
  ps_input_map_del(device->map);
  device->map=map;
  return 0;
}

int ps_input_device_set_premap(struct ps_input_device *device,struct ps_input_premap *premap) {
  if (!device) return -1;
  if (device->premap==premap) return 0;
  if (premap&&(ps_input_premap_ref(premap)<0)) return -1;
  ps_input_premap_del(device->premap);
  device->premap=premap;
  return 0;
}

/* What kind of device?
 */

static int ps_input_device_has_maps_in_0007xxxx(const struct ps_input_device *device) {
  if (!device) return 0;
  if (!device->map) return 0;
  const struct ps_input_map_fld *fld=device->map->fldv;
  int i=device->map->fldc;
  for (;i-->0;fld++) {
    if ((fld->srcbtnid>=0x00070000)&&(fld->srcbtnid<=0x0007ffff)) return 1;
  }
  return 0;
}
 
int ps_input_device_is_joystick(const struct ps_input_device *device) {
  if (!device) return 0;
  switch (device->providerid) {
    // Lots of providers will never give us a joystick.
    case PS_INPUT_PROVIDER_macioc:
    case PS_INPUT_PROVIDER_macwm:
    case PS_INPUT_PROVIDER_x11:
    case PS_INPUT_PROVIDER_mswm:
      return 0;
  }
  if (!device->map) return 0;
  return ps_input_device_has_maps_in_0007xxxx(device)?0:1;
}

int ps_input_device_is_keyboard(const struct ps_input_device *device) {
  if (!device) return 0;
  switch (device->providerid) {
    case PS_INPUT_PROVIDER_macioc: return 0;
    case PS_INPUT_PROVIDER_macwm: return device->map?1:0;
    case PS_INPUT_PROVIDER_machid: return 0;
    case PS_INPUT_PROVIDER_evdev:
      #if PS_ARCH==PS_ARCH_x11
        return 0;
      #elif PS_ARCH==PS_ARCH_raspi
        return ps_input_device_has_maps_in_0007xxxx(device);
      #else
        return 0;
      #endif
    case PS_INPUT_PROVIDER_x11: return device->map?1:0;
    case PS_INPUT_PROVIDER_mswm: return device->map?1:0;
    case PS_INPUT_PROVIDER_mshid: return 0;
  }
  return 0;
}

/* Usable?
 */
 
int ps_input_device_is_usable(const struct ps_input_device *device) {
  if (!device) return 0;
  return ps_input_map_can_support_player(device->map);
}

/* Grow button watcher list.
 */

static int ps_input_device_btnwatch_require(struct ps_input_device *device) {
  if (device->btnwatchc<device->btnwatcha) return 0;
  int na=device->btnwatcha+8;
  if (na>INT_MAX/sizeof(struct ps_input_btnwatch)) return -1;
  void *nv=realloc(device->btnwatchv,sizeof(struct ps_input_btnwatch)*na);
  if (!nv) return -1;
  device->btnwatchv=nv;
  device->btnwatcha=na;
  return 0;
}

/* Button watchers.
 */
 
int ps_input_device_watch_buttons(
  struct ps_input_device *device,
  int (*cb)(struct ps_input_device *device,int btnid,int value,int mapped,void *userdata),
  void (*userdata_del)(void *userdata),
  void *userdata
) {
  if (!device||!cb) return -1;

  if (ps_input_device_btnwatch_require(device)<0) return -1;

  struct ps_input_btnwatch *watch=device->btnwatchv+device->btnwatchc;
  memset(watch,0,sizeof(struct ps_input_btnwatch));

  watch->watchid=device->btnwatchid_next++;
  if (device->btnwatchid_next<1) device->btnwatchid_next=1;
  watch->cb=cb;
  watch->userdata_del=userdata_del;
  watch->userdata=userdata;

  device->btnwatchc++;
  return watch->watchid;
}

int ps_input_device_unwatch_buttons(struct ps_input_device *device,int watchid) {
  if (!device) return -1;
  struct ps_input_btnwatch *watch=device->btnwatchv;
  int i=0; for (;i<device->btnwatchc;i++,watch++) {
    if (watch->watchid==watchid) {
      ps_input_btnwatch_cleanup(watch);
      device->btnwatchc--;
      memmove(watch,watch+1,sizeof(struct ps_input_btnwatch)*(device->btnwatchc-i));
      return 0;
    }
  }
  return -1;
}

int ps_input_device_call_button_watchers(struct ps_input_device *device,int btnid,int value,int mapped) {
  if (!device) return -1;
  int i; for (i=0;i<device->btnwatchc;i++) {
    struct ps_input_btnwatch *watch=device->btnwatchv+i;
    int err=watch->cb(device,btnid,value,mapped,watch->userdata);
    if (err<0) return err;
  }
  return 0;
}

/* Default 'report_buttons' if a report reader is in use.
 */
 
int ps_input_device_report_buttons_via_report_reader(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  if (!device||!cb) return -1;
  if (!device->report_reader) return 0;
  int fldc=ps_input_report_reader_count_fields(device->report_reader);
  int fldix=0; for (;fldix<fldc;fldix++) {
    int fldid=ps_input_report_reader_fldid_by_fldix(device->report_reader,fldix);
    int size=ps_input_report_reader_size_by_fldix(device->report_reader,fldix);
    int sign=ps_input_report_reader_sign_by_fldix(device->report_reader,fldix);
    int value=ps_input_report_reader_value_by_fldix(device->report_reader,fldix);
    struct ps_input_btncfg btncfg={
      .srcbtnid=fldix,
      .value=value,
      .default_usage=fldid,
    };
    if (sign&&(size>1)) {
      btncfg.hi=(1<<(size-1))-1;
      btncfg.lo=-btncfg.hi-1;
    } else {
      btncfg.lo=0;
      btncfg.hi=(1<<size)-1;
    }
    int err=cb(device,&btncfg,userdata);
    if (err) return err;
  }
  return 0;
}
