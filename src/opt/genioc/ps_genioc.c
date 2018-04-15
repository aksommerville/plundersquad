#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_clockassist.h"
#include "os/ps_userconfig.h"
#include "os/ps_fs.h"
#include "os/ps_file_list.h"
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
    int pid=getpid();
    char linkpath[64];
    snprintf(linkpath,sizeof(linkpath),"/proc/%d/exe",pid);
    char tmp[1024];
    int tmpc=readlink(linkpath,tmp,sizeof(tmp));
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      if (tmpc<=dsta) memcpy(dst,tmp,dsta);
      if (tmpc<dsta) dst[tmpc]=0;
      return tmpc;
    }
  
  #endif
  return -1;
}

/* Get neighbor path to executable.
 * Fails if buffer too small.
 */

static int ps_genioc_get_executable_neighbor_path(char *dst,int dsta,const char *suffix) {
  if (!dst||(dsta<0)) dsta=0;
  if (!suffix) return -1;
  char tmp[1024];
  int tmpc=ps_genioc_get_executable_path(tmp,sizeof(tmp));
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  int dstc=ps_file_dirname(dst,dsta,tmp,tmpc);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  int suffixc=0;
  while (suffix[suffixc]) suffixc++;
  if (dstc>=dsta-suffixc-1) return -1;
  dst[dstc++]=PS_PATH_SEPARATOR_CHAR;
  memcpy(dst+dstc,suffix,suffixc);
  dstc+=suffixc;
  dst[dstc]=0;
  return dstc;
}

/* First pass through argv.
 */

static int ps_genioc_argv_prerun(struct ps_userconfig *userconfig,int argc,char **argv) {
  int argp; for (argp=1;argp<argc;argp++) {
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
      if (ps_file_list_add(ps_userconfig_get_config_file_list(userconfig),0,v,vc)<0) return -1;
      argv[argp]="";

    }
  }
  return 0;
}

/* Set possible config paths.
 */

static int ps_genioc_set_default_config_paths(struct ps_userconfig *userconfig) {
  char tmp[1024];
  int tmpc;
  struct ps_file_list *list=ps_userconfig_get_config_file_list(userconfig);
  if (!list) return -1;

  /* Regardless of the platform, we will first look for "plundersquad.cfg" next to the executable.
   */
  if ((tmpc=ps_genioc_get_executable_neighbor_path(tmp,sizeof(tmp),"plundersquad.cfg"))>0) {
    if (ps_file_list_add(list,-1,tmp,tmpc)<0) return -1;
  }

  #if PS_ARCH==PS_ARCH_linux || PS_ARCH==PS_ARCH_raspi
    if (ps_file_list_add(list,-1,"/usr/local/share/plundersquad/plundersquad.cfg",-1)<0) return -1;
    if (ps_file_list_add(list,-1,"/usr/share/plundersquad/plundersquad.cfg",-1)<0) return -1;

  #elif PS_ARCH==PS_ARCH_mswin

  #endif
  return 0;
}

/* Tweak userconfig declarations for this platform.
 */

static int ps_genioc_set_platform_defaults(struct ps_userconfig *userconfig) {
  char tmp[1024];
  int tmpc;
  struct ps_file_list *inputs=ps_userconfig_get_input_file_list(userconfig);
  struct ps_file_list *datas=ps_userconfig_get_data_file_list(userconfig);
  if (!inputs||!datas) return -1;

  /* First look for both files adjacent to the executable.
   */
  if ((tmpc=ps_genioc_get_executable_neighbor_path(tmp,sizeof(tmp),"input.cfg"))>0) {
    if (ps_file_list_add(inputs,-1,tmp,tmpc)<0) return -1;
  }
  if ((tmpc=ps_genioc_get_executable_neighbor_path(tmp,sizeof(tmp),"ps-data"))>0) {
    if (ps_file_list_add(datas,-1,tmp,tmpc)<0) return -1;
  }

  /* Look in some shared places for Linux.
   */
  #if PS_ARCH==PS_ARCH_linux || PS_ARCH==PS_ARCH_raspi
    if (ps_file_list_add(inputs,-1,"/usr/local/share/plundersquad/input.cfg",-1)<0) return -1;
    if (ps_file_list_add(inputs,-1,"/usr/share/plundersquad/input.cfg",-1)<0) return -1;
    if (ps_file_list_add(datas,-1,"/usr/local/share/plundersquad/ps-data",-1)<0) return -1;
    if (ps_file_list_add(datas,-1,"/usr/share/plundersquad/ps-data",-1)<0) return -1;
  #endif

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
  if (ps_genioc_set_default_config_paths(userconfig)<0) return -1;
  if (ps_genioc_set_platform_defaults(userconfig)<0) return -1;
  if (ps_userconfig_commit_paths(userconfig)<0) return -1;
  if (ps_userconfig_load_file(userconfig)<0) return -1;
  if (ps_userconfig_set_dirty(userconfig,0)<0) return -1;
  
  int err=ps_userconfig_load_argv(userconfig,argc,argv);
  if (err<0) return -1;
  if (ps_genioc_assert_configuration(userconfig)<0) return -1;
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
