#include "test/ps_test.h"
#include "opt/machid/ps_machid.h"
#include <unistd.h>

#define EVA 50
static int evc[EVA]={0};
static int evoc=0;

static int ps_test_machid_test_device(void *hiddevice,int vid,int pid,int usagepage,int usage) {
  PS_LOG("test device %p vid=0x%04x pid=0x%04x usage=0x%04x%04x",hiddevice,vid,pid,usagepage,usage);
  if ((vid==0x20d6)&&(pid==0xca6d)) return 1; // BDA Pro Ex
  return 0;
}

static int ps_test_machid_connect(int devid) {
  PS_LOG("connect %d",devid);

  int btnc=ps_machid_dev_count_buttons(devid);
  PS_LOG("button count %d",btnc);
  int btnix=0; for (;btnix<btnc;btnix++) {
    int err,btnid,usage,lo,hi,value;
    if ((err=ps_machid_dev_get_button_info(&btnid,&usage,&lo,&hi,&value,devid,btnix))<0) {
      PS_LOG("ERROR %d retrieving btnix %d",err,btnix);
    } else {
      PS_LOG("[%d] id=%d usage=0x%08x range=[%d..%d] value=%d",btnix,btnid,usage,lo,hi,value);
    }
  }
  
  return 0;
}

static int ps_test_machid_disconnect(int devid) {
  PS_LOG("disconnect %d",devid);
  return 0;
}

static int ps_test_machid_button(int devid,int btnid,int value) {
  //PS_LOG("button %d.%d=%d",devid,btnid,value);

  if ((btnid>=0)&&(btnid<EVA)) evc[btnid]++;
  else evoc++;

  /* BDA Pro Ex is extremely noisy. Ignore some buttons...
   */
  switch (btnid) {
    case 15: return 0; // dpad as single enum
    case 16: case 17: case 18: case 19: return 0; // ?
    case 20: case 21: case 22: case 23: return 0; // analogue dpad
    case 24: case 25: case 26: case 27: return 0; // Right thumb buttons, analogue
    case 28: case 29: case 30: case 31: return 0; // analogue triggers
    case 32: case 33: case 34: case 35: {
        if ((value>=507)&&(value<=516)) return 0;
      } break;
  }

  PS_LOG("button %d.%d=%d",devid,btnid,value);

  return 0;
}

PS_TEST(machid,ignore,machid) {
  ps_machid_quit();
  
  struct ps_machid_delegate delegate={
    .test_device=ps_test_machid_test_device,
    .connect=ps_test_machid_connect,
    .disconnect=ps_test_machid_disconnect,
    .button=ps_test_machid_button,
  };
  PS_ASSERT_CALL(ps_machid_init(&delegate))

  PS_LOG("Running for about 5 seconds...");
  int ttl=300;
  while (ttl-->0) {
    usleep(16000);
    PS_ASSERT_CALL(ps_machid_update());
  }

  int i=0; for (;i<EVA;i++) {
    if (evc[i]) PS_LOG("Button %d updated %d times.",i,evc[i]);
  }
  if (evoc) PS_LOG("Buttons out of range (0..%d) updated %d times.",EVA-1,evoc);
    
  ps_machid_quit();
  return 0;
}
