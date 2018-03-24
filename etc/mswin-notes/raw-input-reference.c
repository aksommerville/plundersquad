#include "skeleton_wingdi_internal.h"

/* HIDP_PREPARSEDDATA constants and helpers.
 */

#define SKELETON_HIDP_HEADER_SIZE 36
#define SKELETON_HIDP_USAGE_SIZE 104

#define SKELETON_HIDP_REPORT_COUNT_SANITY_LIMIT 256
#define SKELETON_HIDP_REPORT_SIZE_SANITY_LIMIT 32

#define RD16(src,p) ((((uint8_t*)(src))[p])|(((uint8_t*)(src))[p+1]<<8))
#define RD32(src,p) (RD16(src,p)|(RD16(src,p+2)<<16))

/* HIDP_PREPARSEDDATA header.
 * Returns count of usage descriptors to expect.
 */

static int skeleton_wingdi_hid_apply_header(struct skeleton_input_rdev *rdev,const void *src,int srcc) {
  if (srcc<SKELETON_HIDP_HEADER_SIZE) return -1;
  
  if (skeleton_input_rdev_set_hid_usage(rdev,(RD16(src,2)<<16)|RD16(src,0))<0) return -1;

  /* TODO: One of these I think is the report length, but I don't know which.
   * Should be at least one is report length and at least one the usage count.
   * For the device I dumped, these are both '9'.
   * In truth, though, it doesn't matter: Incoming reports are pre-measured.
   * Usage count doesn't matter either, since we have the total size of usage descriptors.
   * UPDATE: Having observed a few other devices, it seems that 0x0e is report size and 0x1c is usage count.
   * Still doesn't matter.
  int fld_0e=RD16(src,0x0e); // Report size
  int fld_14=RD16(src,0x14);
  int fld_18=RD16(src,0x18);
  int fld_1c=RD16(src,0x1c); // Usage count
  if ((fld_0e!=fld_14)||(fld_0e!=fld_18)||(fld_0e!=fld_1c)) {
    skeleton_log_info("*** Disparate values in potential report-size fields: 0x0e=%d 0x14=%d 0x18=%d 0x1c=%d",fld_0e,fld_14,fld_18,fld_1c);
  } else {
    skeleton_log_info("HIDP_PREPARSEDDATA, potential report-size fields all have value '%d'",fld_0e);
  }
  */

  int usage_descriptors_size=RD16(src,0x20);
  int usage_descriptor_count=usage_descriptors_size/SKELETON_HIDP_USAGE_SIZE;
  
  return usage_descriptor_count;
}

/* HIDP_PREPARSEDDATA footer.
 * I think there is no useful information here.
 * Size will vary.
 */

static int skeleton_wingdi_hid_apply_footer(struct skeleton_input_rdev *rdev,const void *src,int srcc) {
  return 0;
}

/* HIDP_PREPARSEDDATA single usage record.
 * Return >0 if valid, 0 if invalid, or <0 for real errors.
 */

static int skeleton_wingdi_hid_apply_usage(struct skeleton_input_rdev *rdev,const void *src,int srcc) {
  if (!rdev||!src||(srcc<SKELETON_HIDP_USAGE_SIZE)) return -1;

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
  if (report_size>SKELETON_HIDP_REPORT_SIZE_SANITY_LIMIT) return 0;
  if (report_count>SKELETON_HIDP_REPORT_COUNT_SANITY_LIMIT) return 0;

  /* Define report elements. */
  uint16_t usage=usage_low;
  uint16_t index=index_low;
  int offset=(low_offset<<3)+shift;
  int i; for (i=0;i<report_count;i++) {
    if (usage_page==0xff00) {
      skeleton_log_debug("rdev %p, skipping usage %04x/%04x due to vendor-defined page",rdev,usage_page,usage);
      continue;
    }
    if (skeleton_input_rdev_add_btndesc(rdev,offset,report_size,(usage_page<<16)|usage)<0) return -1;
    if (usage<usage_high) usage++;
    if (index<index_high) index++;
    offset+=report_size;
  }

  return 1;
}

/* Apply HIDP_PREPARSEDDATA to HID.
 */

static int skeleton_wingdi_setup_hid(struct skeleton_input_rdev *rdev,const void *src,int srcc) {
  skeleton_log_trace("skeleton_wingdi_setup_hid %p %p %d",rdev,src,srcc);
  if (!rdev) return -1;
  if (!src) return 0;
  if ((srcc<8)||memcmp(src,"HidP KDR",8)) return 0;

  int srcp=8;
  if (srcp>srcc-SKELETON_HIDP_HEADER_SIZE) return 0;
  int usagec=skeleton_wingdi_hid_apply_header(rdev,(char*)src+srcp,SKELETON_HIDP_HEADER_SIZE);
  if (usagec<0) return -1;
  srcp+=SKELETON_HIDP_HEADER_SIZE;

  while ((srcp<=srcc-SKELETON_HIDP_USAGE_SIZE)&&(usagec-->0)) {
    int err=skeleton_wingdi_hid_apply_usage(rdev,(char*)src+srcp,SKELETON_HIDP_USAGE_SIZE);
    if (err<0) return -1;
    if (!err) break;
    srcp+=SKELETON_HIDP_USAGE_SIZE;
  }

  if (skeleton_wingdi_hid_apply_footer(rdev,(char*)src+srcp,srcc-srcp)<0) return -1;

  return 0;
}

/* Create and initialize HID.
 */

static int skeleton_wingdi_initialize_raw_device(struct skeleton_input_rdev *rdev,RAWINPUT *raw) {

  void *ppdata=0;
  UINT ppdataa=0;
  int ppdatac=GetRawInputDeviceInfo(raw->header.hDevice,RIDI_PREPARSEDDATA,ppdata,&ppdataa);
  if (ppdataa>0) {
    if (!(ppdata=calloc(1,ppdataa))) return -1;
    ppdatac=GetRawInputDeviceInfo(raw->header.hDevice,RIDI_PREPARSEDDATA,ppdata,&ppdataa);
  }

  if (ppdata&&(ppdatac>0)&&(ppdatac<=ppdataa)) {
    if (skeleton_wingdi_setup_hid(rdev,ppdata,ppdatac)<0) {
      skeleton_log_error("Failed to setup HID device. Device will be preserved as a dummy.");
    }
  } else {
    skeleton_log_error("No HID descriptor found for device. Device will be preserved as a dummy.");
  }

  if (ppdata) free(ppdata);
  return 0;
}

/* Callback from raw devices.
 */

static int skeleton_wingdi_raw_callback(struct skeleton_input_rdev *rdev,int btnix,int value) {
  // Even at TRACE level, this is shit-ton of useless logging:
  //skeleton_log_trace("%s %p.%d=%d",__func__,rdev,btnix,value);
  //TODO forward joystick event
  return 0;
}

/* Find device definition, create it if necessary.
 */
 
static struct skeleton_input_rdev *skeleton_wingdi_get_rdev(HANDLE device,RAWINPUT *raw) {

  uint64_t rdevid=(uintptr_t)device;
  struct skeleton_input_rdev *rdev=0;
  int i; for (i=0;i<skeleton_wingdi.rdevc;i++) {
    if (skeleton_input_rdev_get_rdevid(skeleton_wingdi.rdevv[i])==rdevid) {
      rdev=skeleton_wingdi.rdevv[i];
      break;
    }
  }

  if (!rdev) {
    if (skeleton_wingdi.rdevc>=skeleton_wingdi.rdeva) {
      int na=skeleton_wingdi.rdeva+4;
      if (na>INT_MAX/sizeof(void*)) return 0;
      void *nv=realloc(skeleton_wingdi.rdevv,sizeof(void*)*na);
      if (!nv) return 0;
      skeleton_wingdi.rdevv=nv;
      skeleton_wingdi.rdeva=na;
    }
    if (!(rdev=skeleton_input_rdev_new())) return 0;
    skeleton_input_rdev_set_rdevid(rdev,rdevid);
    skeleton_input_rdev_set_callback(rdev,skeleton_wingdi_raw_callback);
    skeleton_wingdi.rdevv[skeleton_wingdi.rdevc++]=rdev;
    if (skeleton_wingdi_initialize_raw_device(rdev,raw)<0) return 0;
  }

  return rdev;
}

/* Raw input event.
 */

static int skeleton_wingdi_evt_raw_keyboard(RAWINPUT *raw) {
/* USHORT MakeCode;
        USHORT Flags;
        USHORT Reserved;
        USHORT VKey;
        UINT Message;
        ULONG ExtraInformation;
*/
  skeleton_log_trace("skeleton_wingdi_evt_raw_keyboard MakeCode=0x%04x Flags=0x%04x VKey=0x%04x Message=0x%08x Extra=0x%016llx",
    raw->data.keyboard.MakeCode,raw->data.keyboard.Flags,raw->data.keyboard.VKey,raw->data.keyboard.Message,raw->data.keyboard.ExtraInformation);
  return 0;
}

static int skeleton_wingdi_evt_raw_mouse(RAWINPUT *raw) {
  skeleton_log_trace("skeleton_wingdi_evt_raw_mouse");
  return 0;
}

static int skeleton_wingdi_evt_raw_hid(RAWINPUT *raw) {

  struct skeleton_input_rdev *rdev=skeleton_wingdi_get_rdev(raw->header.hDevice,raw);
  if (!rdev) return -1;

  /* There is usually (always?) a two-byte report preamble which was not described. */
  const uint8_t *rpt=(uint8_t*)&raw->data.hid.bRawData;
  int rptc=raw->data.hid.dwSizeHid;
  if (rptc>=2) {
    rpt+=2;
    rptc-=2;
  }

  if (skeleton_input_rdev_set_report(rdev,rpt,rptc)<0) return -1;
  
  return 0;
}

/* Receive raw input event, main entry point.
 */

int skeleton_wingdi_raw_input(WPARAM wparam,LPARAM lparam) {
  uint8_t buf[1024];
  UINT bufa=sizeof(buf);
  int bufc=GetRawInputData((HRAWINPUT)lparam,RID_INPUT,buf,&bufa,sizeof(RAWINPUTHEADER));
  if ((bufc<0)||(bufc>sizeof(buf))) {
    skeleton_log_error("GetRawInputData: %d",bufc);
    return 0;
  }
  RAWINPUT *raw=(RAWINPUT*)buf;
  switch (raw->header.dwType) {
    case RIM_TYPEKEYBOARD: return skeleton_wingdi_evt_raw_keyboard(raw);
    case RIM_TYPEMOUSE: return skeleton_wingdi_evt_raw_mouse(raw);
    case RIM_TYPEHID: return skeleton_wingdi_evt_raw_hid(raw);
    default: skeleton_log_info("Raw input header type %d",raw->header.dwType);
  }
  return 0;
}
