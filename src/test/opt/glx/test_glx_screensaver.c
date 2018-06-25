#include "test/ps_test.h"
#include "opt/glx/ps_glx.h"
#include "opt/glx/ps_glx_internal.h"
#include "os/ps_clockassist.h"

/* 2018-06-24
 * - In MATE Control Center, Power Management, set "Put display to sleep when inactive for:" = "1 minute".
 *   ...about 1:40 of idle time, and they do go to sleep.
 * - Set up to do nothing and delay for 2:00. Display did go to sleep while test was running, as expected.
 * - Same test, with an initial call to XSetScreenSaver(0)...
 *   ...XSetScreenSaver(0) didn't change anything. Display went to sleep after about 1:40 as in previous test.
 *   ...Not sure what to make of that, but complete disablement isn't what we want anyway.
 * - Adding XForceScreenSaver(ScreenSaverReset) every 900 ms or so.
 *   ...Expectation: Displays will not sleep during program run. They will fall asleep about 1:40 after termination.
 *   ...Result: Precisely as expected.
 * - Same test without logging, just to be sure...
 *   ...Same result. We have a winner.
 */

/* None of this matters to Windows.
 * I don't know what it looks at exactly, but somehow when the program is running the display never sleeps.
 */

/* This shouldn't matter to raspi but do confirm. TODO
 */

/* I do expect similar trouble with MacOS. TODO
 */
 
static void delay_seconds(int s) {
  ps_log(TEST,INFO,"Delaying for %d seconds...",s);
  int64_t endtime=ps_time_now()+s*1000000;
  int reset_interval=900000;
  int64_t next_reset_time=ps_time_now()+reset_interval;
  while (1) {
    ps_time_sleep(1000000);
    ps_glx_swap(); // For no particular reason.
    int64_t now=ps_time_now();
    if (now>=endtime) return;
    
    // Fire resets as if input were received:
    if (now>=next_reset_time) {
      //ps_log(GLX,INFO,"XForceScreenSaver(ScreenSaverReset)");
      XForceScreenSaver(ps_glx.dpy,ScreenSaverReset);
      next_reset_time=now+reset_interval;
    }
  }
}

/* Main entry point.
 */
 
PS_TEST(test_glx_screensaver,ignore) {
  ps_glx_quit();
  PS_ASSERT_CALL(ps_glx_init(400,300,0,"GLX Screen Saver Test"))
  PS_ASSERT_CALL(ps_glx_swap());
  
  if (0) {
    // This does power down the monitors immediately, and persists after the program terminates.
    ps_log(GLX,INFO,"XForceScreenSaver(ScreenSaverActive)");
    PS_ASSERT_CALL(XForceScreenSaver(ps_glx.dpy,ScreenSaverActive))
  }
  
  if (0) {
    // UNTESTED
    // I think this is the call to disable screen saver.
    // - Confirm it does in fact disable the screen saver.
    //   ...it does not
    // TODO: Does that disablement persist after program termination?
    //   ...I don't care
    ps_log(GLX,INFO,"XSetScreenSaver(0)");
    PS_ASSERT_CALL(XSetScreenSaver(ps_glx.dpy,0,0,DefaultBlanking,DefaultExposures))
  }
  
  if (0) {
    //UNTESTED
    // I think this is the call to restore the default screen saver settings.
    // TODO: Don't test until we're reasonably confident of XSetScreenSaver(0).
    ps_log(GLX,INFO,"XSetScreenSaver(-1)");
    PS_ASSERT_CALL(XSetScreenSaver(ps_glx.dpy,-1,0,DefaultBlanking,DefaultExposures))
  }
  
  delay_seconds(120);

  ps_glx_quit();
  return 0;
}
