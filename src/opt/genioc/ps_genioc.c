#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_clockassist.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static struct {
  int quit;
  struct ps_clockassist clockassist;
} ps_genioc={0};

/* First existing file.
 */

static const char *_ps_genioc_first_existing_file(int directory,...) {
  va_list vargs;
  const char *path;
  va_start(vargs,directory);
  while (path=va_arg(vargs,const char*)) {
    struct stat st;
    if (stat(path,&st)>=0) {
      if (directory) {
        if (!S_ISDIR(st.st_mode)) continue;
      } else {
        if (!S_ISREG(st.st_mode)) continue;
      }
      return path;
    }
  }
  return 0;
}

#define ps_genioc_first_existing_directory(...) _ps_genioc_first_existing_file(1,##__VA_ARGS__,(void*)0)
#define ps_genioc_first_existing_file(...) _ps_genioc_first_existing_file(0,##__VA_ARGS__,(void*)0)

/* Set default command line.
 * It's OK if a file can't be found in the default locations.
 * We will read the command line next, and then validate in a separate pass.
 */

static int ps_genioc_default_cmdline(struct ps_cmdline *cmdline) {

  cmdline->resources_path=ps_genioc_first_existing_directory(
    "../plundersquad/src/data",
    "/usr/local/games/plundersquad/data",
    "/usr/games/plundersquad/data",
    "~/plundersquad/data"
  );

  cmdline->input_config_path=ps_genioc_first_existing_file(
    "../plundersquad/etc/input.cfg",
    "/usr/local/games/plundersquad/config/input.cfg",
    "/usr/games/plundersquad/config/input.cfg",
    "~/plundersquad/config/input.cfg"
  );

  return 0;
}

/* Read command-line arguments.
 */

static int ps_genioc_read_cmdline(struct ps_cmdline *cmdline,int argc,char **argv) {
  int argp=1; while (argp<argc) {
    const char *arg=argv[argp++];

    if (!memcmp(arg,"--resources=",12)) {
      cmdline->resources_path=arg+12;

    } else if (!memcmp(arg,"--input=",8)) {
      cmdline->input_config_path=arg+8;
      
    } else if (!strcmp(arg,"--fullscreen")) {
      cmdline->fullscreen=1;
    
    } else if (arg[0]=='-') {
      ps_log(,ERROR,"Unexpected option '%s'",arg);
      return -1;
    } else if (cmdline->saved_game_path) {
      ps_log(,ERROR,"Unexpected argument '%s'",arg);
      return -1;

    } else {
      cmdline->saved_game_path=arg;
    }
  }
  return 0;
}

/* Validate command line.
 */

static int ps_genioc_validate_cmdline(struct ps_cmdline *cmdline) {

  if (!cmdline->resources_path) {
    ps_log(,ERROR,"Resources not found. Copy to a known location or provide '--resources=PATH'.");
    return -1;
  }

  if (!cmdline->input_config_path) {
    //TODO create input config in a sensible location
    ps_log(,ERROR,"Input config not found. Provide '--input=PATH'.");
    return -1;
  }

  return 0;
}

/* Main.
 */
 
int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate) {

  struct ps_cmdline cmdline={0};
  if (ps_genioc_default_cmdline(&cmdline)<0) return 1;
  if (ps_genioc_read_cmdline(&cmdline,argc,argv)<0) return 1;
  if (ps_genioc_validate_cmdline(&cmdline)<0) return 1;

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
