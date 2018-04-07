#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "util/ps_text.h"
#include "akgl/akgl.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if PS_ARCH==PS_ARCH_mswin
  #include <windows.h>
#endif

#if PS_ARCH==PS_ARCH_mswin
  #define PS_PATH_SEPARATOR_CHAR '\\'
#else
  #define PS_PATH_SEPARATOR_CHAR '/'
#endif

static struct {
  int quit;
  struct ps_clockassist clockassist;
  struct ps_userconfig *userconfig;
} ps_genioc={0};

/* Get executable path.
 */

static int ps_genioc_get_executable_path(char *dst,int dsta) {

  #if PS_ARCH==PS_ARCH_mswin
    int dstc=GetModuleFileName(0,dst,dsta);
    ps_log(,DEBUG,"GetModuleFileName(%d): %d '%.*s'",dsta,dstc,((dstc>=0)&&(dstc<dsta))?dstc:0,dst);
    return dstc;
    
  #elif PS_ARCH==PS_ARCH_linux || PS_ARCH==PS_ARCH_raspi
    #error "Get executable path for Linux" //TODO
  
  #endif
  return -1;
}

/* First pass through argv.
 */

static int ps_genioc_argv_prerun(struct ps_userconfig *userconfig,int argc,char **argv) {
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
      if (ps_userconfig_set_path(userconfig,v,vc)<0) return -1;
      argv[argp]="";

    }
  }
  return 0;
}

/* Set config path if not provided with --config
 */

static int ps_genioc_set_default_config_path(struct ps_userconfig *userconfig) {

  /* Regardless of the platform, we will look for "plundersquad.cfg" next to the executable.
   */
  char neighbor[1024];
  int neighborc=ps_genioc_get_executable_path(neighbor,sizeof(neighbor));
  if ((neighborc<1)||(neighborc>=sizeof(neighbor)-17)) {
    neighborc=0;
    neighbor[0]=0;
  } else {
    neighbor[neighborc++]=PS_PATH_SEPARATOR_CHAR;
    memcpy(neighbor+neighborc,"plundersquad.cfg",16);
    neighborc+=16;
    neighbor[neighborc]=0;
  }

  #if PS_ARCH==PS_ARCH_linux || PS_ARCH==PS_ARCH_raspi
    if (ps_userconfig_set_first_existing_path(userconfig,
      neighbor,
      "/usr/local/share/plundersquad/plundersquad.cfg",
      "/usr/share/plundersquad/plundersquad.cfg",
    NULL)<0) {
      if (ps_userconfig_set_path(userconfig,neighbor)<0) return -1;
    }

  #elif PS_ARCH==PS_ARCH_mswin
    if (ps_userconfig_set_first_existing_path(userconfig,
      neighbor,
    NULL)<0) {
      if (ps_userconfig_set_path(userconfig,neighbor)<0) return -1;
    }

  #else
    #error "Please define default config paths for this platform."
  #endif
  return 0;
}

/* Tweak userconfig declarations for this platform.
 */

static int ps_genioc_set_platform_defaults(struct ps_userconfig *userconfig) {

  char exepath[1024];
  int exepathc=ps_genioc_get_executable_path(exepath,sizeof(exepath));
  if ((exepathc>0)&&(exepathc<sizeof(exepath))) {
    exepath[exepathc++]=PS_PATH_SEPARATOR_CHAR;
    
    if (exepathc<=sizeof(exepath)-10) {
      memcpy(exepath+exepathc,"input.cfg",10);
      if (ps_userconfig_set(userconfig,"input",5,exepath,exepathc+9)<0) return -1;
    }

    if (exepathc<=sizeof(exepath)-8) {
      memcpy(exepath+exepathc,"ps-data",8);
      if (ps_userconfig_set(userconfig,"resources",9,exepath,exepathc+7)<0) return -1;
    }
  }

  return 0;
}

/* After reading all config sources, confirm that we have a sane configuration.
 */

static int ps_genioc_assert_configuration(struct ps_userconfig *userconfig) {
  const char *v;
  int vc;

  if ((vc=ps_userconfig_peek_field_as_string(&v,userconfig,ps_userconfig_search_field(userconfig,"input",5)))<0) return -1;
  if (vc<1) {
    ps_log(CONFIG,ERROR,"Input config path unset. Please rerun with '--input=PATH' (we'll remember it after that)");
    return -1;
  }
  if (ps_mkdir_parents(v)<0) return -1;

  if ((vc=ps_userconfig_peek_field_as_string(&v,userconfig,ps_userconfig_search_field(userconfig,"resources",9)))<0) return -1;
  if (vc<1) {
    ps_log(CONFIG,ERROR,"Failed to locate 'ps-data'. Please rerun with '--resources=PATH' (we'll remember it after that)");
    return -1;
  }

  return 0;
}

/* Configure.
 */

static int ps_genioc_configure(struct ps_userconfig *userconfig,int argc,char **argv) {
  int argp;
  
  if (ps_userconfig_declare_default_fields(userconfig)<0) return -1;
  if (ps_genioc_argv_prerun(userconfig,argc,argv)<0) return -1;
  if (!ps_userconfig_get_path(userconfig)) {
    if (ps_genioc_set_default_config_path(userconfig)<0) return -1;
  }
  if (ps_genioc_set_platform_defaults(userconfig)<0) return -1;
  if (ps_userconfig_load_file(userconfig)<0) return -1;
  if (ps_userconfig_set_dirty(userconfig,0)<0) return -1;
  
  int err=ps_userconfig_load_argv(userconfig,argc,argv);
  if (err<0) return -1;
  if (ps_genioc_assert_config(userconfig)<0) return -1;
  if (err) {
    if (ps_userconfig_save_file(userconfig)<0) return -1;
  }

  return 0;
}

/* Main.
 */
 
int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate) {

  if (!(ps_genioc.userconfig=ps_userconfig_new())) return 1;
  if (ps_genioc_configure(ps_genioc.userconfig,argc,argv)<0) return 1;

  if (delegate->init(ps_genioc.userconfig)<0) {
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
