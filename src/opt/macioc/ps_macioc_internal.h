#ifndef PS_MACIOC_INTERNAL_H
#define PS_MACIOC_INTERNAL_H

#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_clockassist.h"
#include "input/ps_input.h"
#include <Cocoa/Cocoa.h>

@interface AKAppDelegate : NSObject <NSApplicationDelegate> {
}
@end

extern struct ps_macioc {
  int init;
  struct ps_ioc_delegate delegate;
  int terminate;
  int focus;
  int update_in_progress;
} ps_macioc;

void ps_macioc_abort(const char *fmt,...);
int ps_macioc_call_init();
void ps_macioc_call_quit();

#endif
