#include "ps_macioc_internal.h"
#include "os/ps_fs.h"
#include "os/ps_file_list.h"
#include "util/ps_text.h"
#include "akgl/akgl.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct ps_macioc ps_macioc={0};

/* Log to a text file. Will work even if the TTY is unset.
 */
#if 0 // I'll just leave this here in case we need it again...
static void ps_macioc_surelog(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char message[256];
  int messagec=vsnprintf(message,sizeof(message),fmt,vargs);
  if ((messagec<0)||(messagec>=sizeof(message))) {
    messagec=snprintf(message,sizeof(message),"(unable to generate message)");
  }
  int f=open("/Users/andy/proj/plundersquad/surelog",O_WRONLY|O_APPEND|O_CREAT,0666);
  if (f<0) return;
  int err=write(f,message,messagec);
  close(f);
}

#define SURELOG(fmt,...) ps_macioc_surelog("%s:%d:%s: "fmt"\n",__FILE__,__LINE__,__func__,##__VA_ARGS__);
#endif

/* Get a path under the main bundle's "Resources" directory.
 */

static int ps_macioc_get_resource_path(char *dst,int dsta,const char *suffix) {
#if 0 // XXX We now cache the resources path globally.
  /* Copy bundle's resourcePath to dst. */
  NSBundle *bundle=[NSBundle mainBundle];
  const char *src=[[bundle resourcePath] UTF8String];
  if (!src) return -1;
  int srcc=0; while (src[srcc]) srcc++;
  if (!srcc) return -1;
  if (srcc>=dsta) return -1;
  memcpy(dst,src,srcc);
  int dstc=srcc;
  if ((dstc>0)&&(dst[dstc-1]!='/')) dst[dstc++]='/';

  /* Measure suffix and ensure it fits with terminator -- error if not. */
  int suffixc=0;
  if (suffix) while (suffix[suffixc]) suffixc++;
  if (dstc>=dsta-suffixc) return -1;
  memcpy(dst+dstc,suffix,suffixc);
  dstc+=suffixc;

  dst[dstc]=0;
  return dstc;
#endif

  if (!dst||(dsta<0)) dsta=0;
  int dstc=snprintf(dst,dsta,"%.*s/%s",ps_macioc.path_resourcesc,ps_macioc.path_resources,suffix);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

static int ps_macioc_get_preferences_path(char *dst,int dsta,const char *suffix) {
  if (!dst||(dsta<0)) dsta=0;
  int dstc=snprintf(dst,dsta,"%.*s/%s",ps_macioc.path_preferencesc,ps_macioc.path_preferences,suffix);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* If a file doesn't exist in ~/Library/Preferences/com.aksommerville.plundersquad, copy it from the app bundle.
 */

static int ps_macioc_ensure_preferences_file(const char *basename) {

  char dstpath[1024];
  int dstpathc=snprintf(dstpath,sizeof(dstpath),"%.*s/%s",ps_macioc.path_preferencesc,ps_macioc.path_preferences,basename);
  if ((dstpathc<1)||(dstpathc>=sizeof(dstpath))) return -1;

  struct stat st;
  if (stat(dstpath,&st)>=0) return 0;
  
  char srcpath[1024];
  int srcpathc=snprintf(srcpath,sizeof(srcpath),"%.*s/%s",ps_macioc.path_resourcesc,ps_macioc.path_resources,basename);
  if ((srcpathc<1)||(srcpathc>=sizeof(srcpath))) return -1;

  void *src=0;
  int srcc=ps_file_read(&src,srcpath);
  if ((srcc<0)||!src) {
    ps_log(MACIOC,ERROR,"%s: failed to copy file to preferences",srcpath);
    return -1;
  }
  if (ps_file_write(dstpath,src,srcc)<0) {
    ps_log(MACIOC,ERROR,"%s: failed to copy file to preferences",dstpath);
    free(src);
    return -1;
  }
  free(src);

  return 0;
}

/* Check whether the preferences directory exists.
 * Copy from app bundle as needed.
 * The volatile config files in our app bundle must not be changed, otherwise we would break the code signature.
 */

static int ps_macioc_copy_resources_to_preferences() {

  /* Acquire resources path. */
  if (!ps_macioc.path_resources) {
    const char *src=[[[NSBundle mainBundle] resourcePath] UTF8String];
    if (!src) {
      ps_log(MACIOC,FATAL,"Failed to acquire bundle resource path.");
      return -1;
    }
    int srcc=0;
    while (src[srcc]) srcc++;
    if (!(ps_macioc.path_resources=malloc(srcc+1))) return -1;
    memcpy(ps_macioc.path_resources,src,srcc);
    ps_macioc.path_resources[srcc]=0;
    ps_macioc.path_resourcesc=srcc;
  }

  /* Acquire preferences path. */
  if (!ps_macioc.path_preferences) {
    const char *sfx="/Library/Preferences/com.aksommerville.plundersquad";
    const char *pfx="";
    const char *src=getenv("HOME");
    if (!src||!src[0]) {
      src=getenv("USER");
      if (!src||!src[0]) {
        ps_log(MACIOC,FATAL,"Failed to acquire home directory.");
        return -1;
      }
      pfx="/Users/";
    }
    int pfxc=0; while (pfx[pfxc]) pfxc++;
    int srcc=0; while (src[srcc]) srcc++;
    int sfxc=0; while (sfx[sfxc]) sfxc++;
    int dstc=pfxc+srcc+sfxc;
    if (!(ps_macioc.path_preferences=malloc(dstc+1))) return -1;
    memcpy(ps_macioc.path_preferences,pfx,pfxc);
    memcpy(ps_macioc.path_preferences+pfxc,src,srcc);
    memcpy(ps_macioc.path_preferences+pfxc+srcc,sfx,sfxc);
    ps_macioc.path_preferences[dstc]=0;
    ps_macioc.path_preferencesc=dstc;
  }

  /* Create preferences directory if not existing. */
  if (ps_mkdir(ps_macioc.path_preferences)<0) {
    ps_log(MACIOC,FATAL,"%s: Failed to create preferences directory.",ps_macioc.path_preferences);
    return -1;
  }

  /* Copy each of the volatile files if it doesn't exist in preferences. */
  if (ps_macioc_ensure_preferences_file("plundersquad.cfg")<0) return -1;
  if (ps_macioc_ensure_preferences_file("input.cfg")<0) return -1;
  //if (ps_macioc_ensure_preferences_file("highscores")<0) return -1; // highscores is optional and doesn't exist in the bundle

  return 0;
}

/* First pass through argv.
 */

static int ps_macioc_argv_prerun(int argc,char **argv) {
  int argp;
  for (argp=1;argp<argc;argp++) {
    const char *k=argv[argp];
    if (!k) continue;
    if ((k[0]!='-')||(k[1]!='-')||!k[2]) continue;
    k+=2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=k+kc;
    int vc=0;
    if (v[0]=='=') {
      v++;
      while (v[vc]) vc++;
    }

    if ((kc==10)&&!memcmp(k,"reopen-tty",10)) {
      if (ps_os_reopen_tty(v)<0) return -1;
      argv[argp]="";

    } else if ((kc==5)&&!memcmp(k,"chdir",5)) {
      if (chdir(v)<0) return -1;
      argv[argp]="";

    } else if ((kc==6)&&!memcmp(k,"config",6)) {
      struct ps_file_list *list=ps_userconfig_get_config_file_list(ps_macioc.userconfig);
      if (ps_file_list_add(list,0,v,vc)<0) return -1;
      argv[argp]="";

    }
  }
  return 0;
}

/* Other defaults, before config file.
 */

static int ps_macioc_set_defaults() {
  char path[1024];
  int pathc;

  if ((pathc=ps_macioc_get_preferences_path(path,sizeof(path),"plundersquad.cfg"))<0) return -1;
  if (ps_file_list_add(ps_userconfig_get_config_file_list(ps_macioc.userconfig),-1,path,pathc)<0) return -1;

  if ((pathc=ps_macioc_get_preferences_path(path,sizeof(path),"input.cfg"))<0) return -1;
  if (ps_file_list_add(ps_userconfig_get_input_file_list(ps_macioc.userconfig),-1,path,pathc)<0) return -1;

  if ((pathc=ps_macioc_get_resource_path(path,sizeof(path),"ps-data"))<0) return -1;
  if (ps_file_list_add(ps_userconfig_get_data_file_list(ps_macioc.userconfig),-1,path,pathc)<0) return -1;

  if ((pathc=ps_macioc_get_preferences_path(path,sizeof(path),"highscores"))<0) return -1;
  if (ps_file_list_add(ps_userconfig_get_highscores_file_list(ps_macioc.userconfig),-1,path,pathc)<0) return -1;

  return 0;
}

/* Configure.
 */

static int ps_macioc_configure(int argc,char **argv) {
  int argp;

  if (ps_macioc_copy_resources_to_preferences()<0) return -1;

  if (ps_userconfig_declare_default_fields(ps_macioc.userconfig)<0) return -1;
  if (ps_macioc_argv_prerun(argc,argv)<0) return -1;
  if (ps_macioc_set_defaults()<0) return -1;
  if (ps_userconfig_load_file(ps_macioc.userconfig)<0) return -1;
  if (ps_userconfig_set_dirty(ps_macioc.userconfig,0)<0) return -1;

  int err=ps_userconfig_load_argv(ps_macioc.userconfig,argc,argv);
  if (err<0) return -1;
  if (ps_userconfig_commit_paths(ps_macioc.userconfig)<0) return -1;
  if (err) {
    if (ps_userconfig_save_file(ps_macioc.userconfig)<0) return -1;
  }

  ps_log(MACIOC,INFO,"Resources: %.*s",ps_macioc.path_resourcesc,ps_macioc.path_resources);
  ps_log(MACIOC,INFO,"Preferences: %.*s",ps_macioc.path_preferencesc,ps_macioc.path_preferences);

  return 0;
}

/* Main.
 */

int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate) {

  if (ps_macioc.init) return 1;
  memset(&ps_macioc,0,sizeof(struct ps_macioc));
  ps_macioc.init=1;
  
  if (delegate) memcpy(&ps_macioc.delegate,delegate,sizeof(struct ps_ioc_delegate));

  if (!(ps_macioc.userconfig=ps_userconfig_new())) return 1;
  if (ps_macioc_configure(argc,argv)<0) return 1;

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
  int result=(ps_macioc.delegate.init?ps_macioc.delegate.init(ps_macioc.userconfig):0);
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
     * Update:
     *   !!! After upgrading from 10.11 to 10.13, all the timing got fucked.
     *   Switching to 'waitUntilDone:1' seems to fix it.
     *   If the only problem that way in 10.11 was choppy window resizing, so be it.
     *   Resize seems OK with '1' and OS 10.13.
     */
    [self performSelectorOnMainThread:@selector(updateMain:) withObject:nil waitUntilDone:1];
  
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
