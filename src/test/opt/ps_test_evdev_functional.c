#include "test/ps_test.h"
#include "opt/evdev/ps_evdev.h"
#include <unistd.h>
#include <linux/input.h>

/* Callbacks.
 */

// Don't report more than so many capabilities per type.
#define TEST_EVDEV_CAP_LIMIT 16

struct test_evdev_capctx {
  int keyc;
  int relc;
  int absc;
};

static int test_evdev_cb_capability(int devid,int type,int code,int lo,int hi,int value,void *userdata) {
  struct test_evdev_capctx *ctx=userdata;
  int log=1;
  switch (type) {
    case EV_KEY: if (++(ctx->keyc)>TEST_EVDEV_CAP_LIMIT) log=0; break;
    case EV_ABS: if (++(ctx->absc)>TEST_EVDEV_CAP_LIMIT) log=0; break;
    case EV_REL: if (++(ctx->relc)>TEST_EVDEV_CAP_LIMIT) log=0; break;
  }
  if (log) ps_log(INPUT,DEBUG,"  %d.%d [%d..%d] =%d",type,code,lo,hi,value);
  return 0;
}

static int test_evdev_cb_connect(int devid) {
  ps_log(INPUT,DEBUG,"%s %d",__func__,devid);
  ps_log(INPUT,DEBUG,"  name: %s",ps_evdev_get_name(devid));

  int bustype=0,vendor=0,product=0,version=0;
  int evid=ps_evdev_get_id(&bustype,&vendor,&product,&version,devid);
  ps_log(INPUT,DEBUG,"  evid: %d",evid);
  ps_log(INPUT,DEBUG,"  bustype: %d",bustype);
  ps_log(INPUT,DEBUG,"  vendor: 0x%04x",vendor);
  ps_log(INPUT,DEBUG,"  product: 0x%04x",product);
  ps_log(INPUT,DEBUG,"  version: 0x%04x",version);

  struct test_evdev_capctx ctx={0};
  ps_evdev_report_capabilities(devid,test_evdev_cb_capability,&ctx);
  if (ctx.keyc>TEST_EVDEV_CAP_LIMIT) ps_log(INPUT,DEBUG,"  +%d more KEY capabilities",ctx.keyc-TEST_EVDEV_CAP_LIMIT);
  if (ctx.absc>TEST_EVDEV_CAP_LIMIT) ps_log(INPUT,DEBUG,"  +%d more ABS capabilities",ctx.absc-TEST_EVDEV_CAP_LIMIT);
  if (ctx.relc>TEST_EVDEV_CAP_LIMIT) ps_log(INPUT,DEBUG,"  +%d more REL capabilities",ctx.relc-TEST_EVDEV_CAP_LIMIT);
  
  ps_log(INPUT,DEBUG,"End of cb_connect");
  return 0;
}

static int test_evdev_cb_disconnect(int devid,int reason) {
  ps_log(INPUT,DEBUG,"%s %d %d",__func__,devid,reason);
  return 0;
}

static int test_evdev_cb_event(int devid,int type,int code,int value) {
  ps_log(INPUT,DEBUG,"%s %d.%d:%d=%d",__func__,devid,type,code,value);
  return 0;
}

/* Functional test of evdev, main entry point.
 */

PS_TEST(test_evdev_functional,functional,ignore) {

  ps_evdev_quit();

  ps_log_level_by_domain[PS_LOG_DOMAIN_INPUT]=PS_LOG_LEVEL_TRACE;

  PS_LOG("Initialize evdev...")

  PS_ASSERT_CALL(ps_evdev_init(0,
    test_evdev_cb_connect,
    test_evdev_cb_disconnect,
    test_evdev_cb_event
  ))

  int repc=100;
  int delay=100;
  PS_LOG("Updating at %d-ms intervals %d times (%d s)...",delay,repc,(delay*repc)/1000)
  while (repc-->0) {
    PS_ASSERT_CALL(ps_evdev_update())
    usleep(delay*1000);
  }

  PS_LOG("evdev functional test complete (success)")
  ps_evdev_quit();
  return 0;
}
