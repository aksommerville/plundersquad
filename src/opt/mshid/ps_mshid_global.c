#include "ps_mshid_internal.h"
#include "util/ps_buffer.h"

/* HIDP_PREPARSEDDATA constants and helpers.
 */

#define PS_MSHID_HIDP_HEADER_SIZE 36
#define PS_MSHID_HIDP_USAGE_SIZE 104

#define PS_MSHID_HIDP_REPORT_COUNT_SANITY_LIMIT 256
#define PS_MSHID_HIDP_REPORT_SIZE_SANITY_LIMIT 32

#define RD16(src,p) ((((uint8_t*)(src))[p])|(((uint8_t*)(src))[p+1]<<8))
#define RD32(src,p) (RD16(src,p)|(RD16(src,p+2)<<16))

static struct {
  struct ps_input_provider *provider;
} ps_mshid={0};

/* We use RawInput's device handle as PlunderSquad's device ID.
 * Handle might be 64 bits long, so we XOR its top and bottom halves.
 * Consequently, they are not reversible.
 */

static int ps_mshid_devid_from_handle(HANDLE hDevice) {
  int devid;
  if (sizeof(HANDLE)==8) {
    uint64_t l=(uintptr_t)hDevice;
    devid=(l>>32)^(l&0xffffffff);
  } else {
    devid=(uintptr_t)hDevice;
  }
  return devid&0x7fffffff;
}

/* Update.
 */

static int ps_mshid_update(struct ps_input_provider *provider) {
  // Nothing to do here. We receive events via mswm.
  return 0;
}

/* Initialize.
 */

int ps_mshid_init() {
  if (ps_mshid.provider) return -1;

  if (!(ps_mshid.provider=ps_input_provider_new(0))) return -1;

  ps_mshid.provider->providerid=PS_INPUT_PROVIDER_mshid;
  ps_mshid.provider->update=ps_mshid_update;

  if (ps_input_install_provider(ps_mshid.provider)<0) {
    ps_input_provider_del(ps_mshid.provider);
    ps_mshid.provider=0;
    return -1;
  }
  
  RAWINPUTDEVICE devv[]={
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x04, // joystick
      .dwFlags=0,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x05, // game pad
      .dwFlags=0,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x06, // keyboard
      .dwFlags=0,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x07, // keypad
      .dwFlags=0,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x08, // multi-axis controller, whatever that means
      .dwFlags=0,
    },
  };
  if (!RegisterRawInputDevices(devv,sizeof(devv)/sizeof(RAWINPUTDEVICE),sizeof(RAWINPUTDEVICE))) {
    ps_log(MSHID,ERROR,"Failed to register for raw input.");
    ps_mshid_quit();
    return -1;
  }
  
  return 0;
}

/* Quit.
 */

void ps_mshid_quit() {
  ps_input_uninstall_provider(ps_mshid.provider);
  ps_input_provider_del(ps_mshid.provider);
  ps_mshid.provider=0;
}

/* Apply RIDI_PREPARSEDDATA header to new device.
 * Return usage descriptor count to expect after.
 */

static int ps_mshid_apply_header(struct ps_input_device *device,const uint8_t *src,int srcc) {
  if (srcc<PS_MSHID_HIDP_HEADER_SIZE) return -1;
  
  //if (skeleton_input_rdev_set_hid_usage(rdev,(RD16(src,2)<<16)|RD16(src,0))<0) return -1;
  int usagepage=RD16(src,2);
  int usagecode=RD16(src,0);
  ps_log(MSHID,INFO,"Parsing HID descriptor. Usage=%04x/%04x",usagepage,usagecode);

  int usage_descriptors_size=RD16(src,0x20);
  int usage_descriptor_count=usage_descriptors_size/PS_MSHID_HIDP_USAGE_SIZE;
  
  return usage_descriptor_count;
}

/* HIDP_PREPARSEDDATA single usage record.
 * Return >0 if valid, 0 if invalid, or <0 for real errors.
 */

static int ps_mshid_apply_usage(struct ps_input_device *device,const void *src,int srcc) {
  if (!device||!src||(srcc<PS_MSHID_HIDP_USAGE_SIZE)) return -1;

  /* Decode known fields. */
  uint16_t usage_page=RD16(src,0x00);
  uint8_t shift=((uint8_t*)src)[3];
  uint16_t report_size=RD16(src,0x04);
  uint16_t report_count=RD16(src,0x06);
  uint16_t low_offset=RD16(src,0x08);
  uint16_t total_size=RD16(src,0x0a);
  uint16_t flags=RD16(src,0x0c);
  uint16_t high_offset=RD16(src,0x10);
  uint16_t usage_low=RD16(src,0x3c);
  uint16_t usage_high=RD16(src,0x3e);
  uint16_t index_low=RD16(src,0x48);
  uint16_t index_high=RD16(src,0x4a);

  /* Validate. If it doesn't compute, give up and return zero. */
  if (shift>=8) return 0;
  if (!report_size) return 0;
  if (!report_count) return 0;
  if (report_size*report_count!=total_size) return 0;
  if (report_size>PS_MSHID_HIDP_REPORT_SIZE_SANITY_LIMIT) return 0;
  if (report_count>PS_MSHID_HIDP_REPORT_COUNT_SANITY_LIMIT) return 0;

  /* Define report elements. */
  uint16_t usage=usage_low;
  uint16_t index=index_low;
  int offset=(low_offset<<3)+shift;
  int i; for (i=0;i<report_count;i++) {
    if (usage_page==0xff00) {
      //ps_log(MSHID,DEBUG,"skipping usage %04x/%04x due to vendor-defined page",usage_page,usage);
      continue;
    }

    /* According to the HID 1.1 spec (6.2.2.7), values are signed if the Logical Minimum is less than zero.
     * Problem: I can't seem to find Logical Minimum in this descriptor.
     * So for now at least, we are assuming all input fields to be unsigned.
     * TODO Signed values in MS HID input.
     */
    int sign=0;
    
    if (ps_input_report_reader_add_field_at(
      device->report_reader,offset,report_size,sign,(usage_page<<16)|usage
    )<0) return -1;

    if (usage<usage_high) usage++;
    if (index<index_high) index++;
    offset+=report_size;
  }

  return 1;
}

/* Apply RIDI_PREPARSEDDATA to new device.
 */

static int ps_mshid_apply_preparsed_data(struct ps_input_device *device,const void *src,int srcc) {
  ps_log(MSHID,TRACE,"%s %d, %d bytes",__func__,device->devid,srcc);
  if (!src) return 0;
  if ((srcc<8)||memcmp(src,"HidP KDR",8)) return 0;

  int srcp=8;
  if (srcp>srcc-PS_MSHID_HIDP_HEADER_SIZE) return 0;
  int usagec=ps_mshid_apply_header(device,(char*)src+srcp,PS_MSHID_HIDP_HEADER_SIZE);
  if (usagec<0) return -1;
  srcp+=PS_MSHID_HIDP_HEADER_SIZE;

  while ((srcp<=srcc-PS_MSHID_HIDP_USAGE_SIZE)&&(usagec-->0)) {
    int err=ps_mshid_apply_usage(device,(char*)src+srcp,PS_MSHID_HIDP_USAGE_SIZE);
    if (err<0) return -1;
    if (!err) break;
    srcp+=PS_MSHID_HIDP_USAGE_SIZE;
  }

  //XXX HID footer does not appear useful.
  //if (ps_mshid_apply_footer(device,(char*)src+srcp,srcc-srcp)<0) return -1;
  
  return 0;
}

/* Initialize new device.
 */

static int ps_mshid_GetRawInputDeviceInfo(struct ps_buffer *dst,HANDLE device,int cmd) {
  dst->c=0;
  while (1) {
    UINT len=dst->a;

    // GetRawInputDevice() has the ridiculous requirement that, for RIDI_DEVICEINFO, cbSize must be set in the output buffer.
    // Don't get me started.
    if (cmd==RIDI_DEVICEINFO) {
      int reqsize=sizeof(RID_DEVICE_INFO);
      if (ps_buffer_require(dst,reqsize)<0) return -1;
      RID_DEVICE_INFO *rdi=(RID_DEVICE_INFO*)dst->v;
      memset(rdi,0,reqsize); // Because who knows what other wacky nonsense they'll do...
      rdi->cbSize=reqsize;
      len=reqsize; // Undocumented, but true: GetRawInputDeviceInfo() will fail if you provide *too much* output space.
    }
  
    int c=GetRawInputDeviceInfo(device,cmd,dst->v,&len);
    if (len>dst->a) {
      if (ps_buffer_require(dst,len)<0) return -1;
    } else if (c<0) {
      return -1;
    } else if (c>dst->a) {
      return -1;
    } else {
      dst->c=c;
      return 0;
    }
  }
}

static int ps_mshid_setup_device_preparsed(struct ps_input_device *device,RAWINPUT *evt,struct ps_buffer *buffer) {
  if (ps_mshid_GetRawInputDeviceInfo(buffer,evt->header.hDevice,RIDI_PREPARSEDDATA)<0) return -1;
  if (ps_mshid_apply_preparsed_data(device,buffer->v,buffer->c)<0) return -1;
  return 0;
}

static int ps_mshid_setup_device_name(struct ps_input_device *device,RAWINPUT *evt,struct ps_buffer *buffer) {
  if (ps_mshid_GetRawInputDeviceInfo(buffer,evt->header.hDevice,RIDI_DEVICENAME)<0) return -1;
  
  //ps_log_hexdump(MSHID,DEBUG,buffer->v,buffer->c,"RIDI_DEVICENAME");
  // \\?\HID#VID_0810&PID_E501#6&3dfdfd6&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}
  // ...and a terminating NUL, included in the reported length.
  // Nice, Microsoft. Nice "name".

  HANDLE HIDHandle = CreateFile(buffer->v, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (HIDHandle) {
    char buf[256]={0};
    int result=HidD_GetProductString(HIDHandle,buf,sizeof(buf));
    buf[255]=buf[254]=0;
    ps_input_device_set_name_utf16lez(device,buf);
    CloseHandle(HIDHandle);
  }
  
  return 0;
}

static int ps_mshid_setup_device_info(struct ps_input_device *device,RAWINPUT *evt,struct ps_buffer *buffer) {
  if (ps_mshid_GetRawInputDeviceInfo(buffer,evt->header.hDevice,RIDI_DEVICEINFO)<0) return -1;
  if (buffer->c<(int)sizeof(RID_DEVICE_INFO)) return -1;
  RID_DEVICE_INFO *rdi=(RID_DEVICE_INFO*)buffer->v;
  if (rdi->dwType!=RIM_TYPEHID) return 0;
  device->vendorid=rdi->hid.dwVendorId;
  device->deviceid=rdi->hid.dwProductId;
  return 0;
}

static int ps_mshid_setup_device(struct ps_input_device *device,RAWINPUT *evt) {
  ps_log(MSHID,TRACE,"%s %d",__func__,device->devid);

  if (!(device->report_reader=ps_input_report_reader_new())) return -1;

  struct ps_buffer buffer={0};
  if (
    (ps_mshid_setup_device_preparsed(device,evt,&buffer)<0)||
    (ps_mshid_setup_device_name(device,evt,&buffer)<0)||
    (ps_mshid_setup_device_info(device,evt,&buffer)<0)
  ) {
    // Carry on after errors -- we want to keep the device present so as not to repeatedly reattempt.
  }
  ps_buffer_cleanup(&buffer);

  if (ps_input_report_reader_finish_setup(device->report_reader)<0) return -1;

  return 0;
}

/* Receive report for live device.
 */

static int ps_mshid_rcvrpt_cb(struct ps_input_report_reader *reader,int fldix,int fldid,int v,int pv,void *userdata) {
  struct ps_input_device *device=userdata;
  //ps_log(MSHID,TRACE,"%s devid=%d fldix=%d fldid=%d v=%d pv=%d",__func__,device->devid,fldix,fldid,v,pv);
  if (ps_input_event_button(device,fldix,v)<0) return -1;
  return 0;
}

static int ps_mshid_rcvrpt(struct ps_input_device *device,RAWINPUT *evt) {
  const void *src=&evt->data.hid.bRawData;
  int srcc=evt->data.hid.dwSizeHid;
  if (ps_input_report_reader_set_report(
    device->report_reader,src,srcc,device,ps_mshid_rcvrpt_cb
  )<0) return -1;
  return 0;
}

/* Generic HID event.
 */

static int ps_mshid_evt_hid(RAWINPUT *evt) {

  int devid=ps_mshid_devid_from_handle(evt->header.hDevice);
  if (!devid) {
    ps_log(MSHID,WARNING,"Discarding raw input device due to devid==0");
    return 0;
  }

  int devp=ps_input_provider_device_search(ps_mshid.provider,devid);
  if (devp<0) {
    struct ps_input_device *device=ps_input_device_new(0);
    if (!device) return 0;

    device->devid=devid;
    device->providerid=PS_INPUT_PROVIDER_mshid;
    device->report_buttons=ps_input_device_report_buttons_via_report_reader;

    if (ps_mshid_setup_device(device,evt)<0) {
      ps_log(MSHID,WARNING,"Failed to initialize raw input device.");
      ps_input_device_del(device);
      return 0;
    }

    if (ps_input_provider_install_device(ps_mshid.provider,device)<0) {
      ps_input_device_del(device);
      return -1;
    }

    if (ps_input_report_reader_set_report(
      device->report_reader,&evt->data.hid.bRawData,evt->data.hid.dwSizeHid,0,0
    )<0) return -1;

    if (ps_input_event_connect(device)<0) { //TODO What do RawInput disconnects look like?
      return -1;
    }

  } else {
    if (ps_mshid_rcvrpt(ps_mshid.provider->devv[devp],evt)<0) {
      return -1;
    }
  }
  
  return 0;
}

/* Keyboard and mouse events -- discard.
 */

static int ps_mshid_evt_keyboard(RAWINPUT *evt) {
  //ps_log(MSHID,TRACE,"%s",__func__);
  return 0;
}

static int ps_mshid_evt_mouse(RAWINPUT *evt) {
  //ps_log(MSHID,TRACE,"%s",__func__);
  return 0;
}

/* Receive event.
 */
 
int ps_mshid_event(int wparam,int lparam) {
  //ps_log(MSHID,TRACE,"%s %d,%d",__func__,wparam,lparam);
  uint8_t buf[1024];
  UINT bufa=sizeof(buf);
  int bufc=GetRawInputData((HRAWINPUT)lparam,RID_INPUT,buf,&bufa,sizeof(RAWINPUTHEADER));
  if ((bufc<0)||(bufc>sizeof(buf))) {
    ps_log(MSHID,ERROR,"GetRawInputData: %d",bufc);
    return 0;
  }
  RAWINPUT *raw=(RAWINPUT*)buf;
  //ps_log(MSHID,TRACE,"RawInput: %d bytes, dwType=%d",bufc,raw->header.dwType);
  switch (raw->header.dwType) {
    case RIM_TYPEKEYBOARD: return ps_mshid_evt_keyboard(raw);
    case RIM_TYPEMOUSE: return ps_mshid_evt_mouse(raw);
    case RIM_TYPEHID: return ps_mshid_evt_hid(raw);
    default: ps_log(MSHID,WARNING,"Raw input header type %d",raw->header.dwType);
  }
  return 0;
}
