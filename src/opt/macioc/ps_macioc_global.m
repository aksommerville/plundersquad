#include "ps_macioc_internal.h"
#include <fcntl.h>
#include <unistd.h>

struct ps_macioc ps_macioc={0};

/* Reopen TTY.
 */

static void ps_macioc_reopen_tty(const char *path) {
  int fd=open(path,O_RDWR);
  if (fd<0) return;
  dup2(fd,STDIN_FILENO);
  dup2(fd,STDOUT_FILENO);
  dup2(fd,STDERR_FILENO);
  close(fd);
}

/* Main.
 */

int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate) {

  if (ps_macioc.init) return 1;
  memset(&ps_macioc,0,sizeof(struct ps_macioc));
  ps_macioc.init=1;

  if (delegate) memcpy(&ps_macioc.delegate,delegate,sizeof(struct ps_ioc_delegate));

  int argp=1; while (argp<argc) {
    const char *arg=argv[argp];
    if (!memcmp(arg,"--reopen-tty=",13)) {
      ps_macioc_reopen_tty(arg+13);
      argc--;
      memmove(argv+argp,argv+argp+1,sizeof(void*)*(argc-argp));
    } else if (!memcmp(arg,"--chdir=",8)) {
      chdir(arg+8);
      argc--;
      memmove(argv+argp,argv+argp+1,sizeof(void*)*(argc-argp));
    } else {
      argp++;
    }
  }

  return NSApplicationMain(argc,(const char**)argv);
}

/* Simple termination.
 */
 
void ps_ioc_quit(int force) {
  [NSApplication.sharedApplication terminate:nil];
  ps_log(MACIOC,FATAL,"!!! [NSApplication.sharedApplication terminate:nil] did not terminate execution. Using exit() instead !!!");
  exit(1);
}

/* Abort.
 */
 
void ps_macioc_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    ps_log(MACIOC,FATAL,"%.*s",msgc,msg);
  }
  ps_ioc_quit(1);
}

/* Callback triggers.
 */
 
int ps_macioc_call_init() {
  int result=(ps_macioc.delegate.init?ps_macioc.delegate.init():0);
  ps_macioc.delegate.init=0; // Guarantee only one call.
  return result;
}

void ps_macioc_call_quit() {
  ps_macioc.terminate=1;
  if (ps_macioc.delegate.quit) {
    ps_macioc.delegate.quit();
    ps_macioc.delegate.quit=0; // Guarantee only one call.
  }
}

/* Create system input device.
 */

static int ps_macioc_input_report_buttons(
  struct ps_input_device *device,
  void *userdata,
  int (*cb)(struct ps_input_device *device,const struct ps_input_btncfg *btncfg,void *userdata)
) {
  if (!cb) return -1;
  struct ps_input_btncfg btncfg={
    .srcbtnid=1,
    .lo=0,
    .hi=1,
  };
  return cb(device,&btncfg,userdata);
}

int ps_macioc_connect_input() {
  if (!ps_macioc.init) return -1;
  if (ps_macioc.device_system) return 0;
  if (ps_macioc.provider_system) return 0;

  if (!(ps_macioc.provider_system=ps_input_provider_new(0))) return -1;
  if (!(ps_macioc.device_system=ps_input_device_new(0))) return -1;

  ps_macioc.provider_system->providerid=PS_INPUT_PROVIDER_macioc;
  ps_macioc.provider_system->report_buttons=ps_macioc_input_report_buttons;

  if (ps_input_install_provider(ps_macioc.provider_system)<0) return -1;
  if (ps_input_provider_install_device(ps_macioc.provider_system,ps_macioc.device_system)<0) return -1;
  if (ps_input_event_connect(ps_macioc.device_system)<0) return -1;

  return 0;
}

@implementation AKAppDelegate

/* Main loop.
 * This runs on a separate thread.
 */

-(void)mainLoop:(id)ignore {

  struct ps_clockassist clockassist={0};
  if (ps_clockassist_setup(&clockassist,PS_FRAME_RATE)<0) {
    ps_macioc_abort("Failed to initialize clock.");
  }
  
  while (1) {

    if (ps_macioc.terminate) break;

    if (ps_clockassist_update(&clockassist)<0) {
      ps_macioc_abort("Failed to update clock.");
    }

    if (ps_macioc.terminate) break;

    if (ps_macioc.update_in_progress) {
      //ps_log(MACIOC,TRACE,"Dropping frame due to update still running.");
      continue;
    }

    /* With 'waitUntilDone:0', we will always be on manual timing.
     * I think that's OK. And window resizing is much more responsive this way.
     */
    [self performSelectorOnMainThread:@selector(updateMain:) withObject:nil waitUntilDone:0];
  
  }
}

/* Route call from main loop.
 * This runs on the main thread.
 */

-(void)updateMain:(id)ignore {
  ps_macioc.update_in_progress=1;
  if (ps_macioc.delegate.update) {
    int err=ps_macioc.delegate.update();
    if (err<0) {
      ps_macioc_abort("Error %d updating application.",err);
    }
  }
  ps_macioc.update_in_progress=0;
}

/* Finish launching.
 * We fire the 'init' callback and launch an updater thread.
 */

-(void)applicationDidFinishLaunching:(NSNotification*)notification {
  
  int err=ps_macioc_call_init();
  if (err<0) {
    ps_macioc_abort("Initialization failed (%d). Aborting.",err);
  }

  [NSThread detachNewThreadSelector:@selector(mainLoop:) toTarget:self withObject:nil];
  
}

/* Termination.
 */

-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  return NSTerminateNow;
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return 1;
}

-(void)applicationWillTerminate:(NSNotification*)notification {
  ps_macioc_call_quit();
}

/* Receive system error.
 */

-(NSError*)application:(NSApplication*)application willPresentError:(NSError*)error {
  const char *message=error.localizedDescription.UTF8String;
  ps_log(MACIOC,ERROR,"%s",message);
  return error;
}

/* Change input focus.
 */

-(void)applicationDidBecomeActive:(NSNotification*)notification {
  if (ps_macioc.focus) return;
  ps_macioc.focus=1;
  if (ps_macioc.device_system) {
    if (ps_input_event_button(ps_macioc.device_system,1,1)<0) {
      ps_macioc_abort("Error in application focus callback.");
    }
  }
}

-(void)applicationDidResignActive:(NSNotification*)notification {
  if (!ps_macioc.focus) return;
  ps_macioc.focus=0;
  if (ps_macioc.device_system) {
    if (ps_input_event_button(ps_macioc.device_system,1,0)<0) {
      ps_macioc_abort("Error in application focus callback.");
    }
  }
}

@end
