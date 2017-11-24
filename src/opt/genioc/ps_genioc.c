#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_clockassist.h"

static struct {
  int quit;
  struct ps_clockassist clockassist;
} ps_genioc={0};

/* Main.
 */
 
int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate) {

  struct ps_cmdline cmdline={0};
  if (argc>=2) {
    cmdline.saved_game_path=argv[1];
  }

  if (delegate->init(&cmdline)<0) {
    return 1;
  }
  if (ps_clockassist_setup(&ps_genioc.clockassist,PS_FRAME_RATE)<0) return 1;

  while (!ps_genioc.quit) {
    if (ps_clockassist_update(&ps_genioc.clockassist)<0) return 1;
    if (delegate->update()<0) return 1;
  }

  delegate->quit();
  return 0;
}

/* Terminate the program.
 */
 
void ps_ioc_quit(int force) {
  if (force) {
    exit(1);
  } else {
    ps_genioc.quit=1;
  }
}
