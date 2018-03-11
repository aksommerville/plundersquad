#include "ps.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "os/ps_fs.h"

/* Command-line arguments.
 * Nothing fancy here. We take two arguments, OUTPUT and INPUT.
 */
 
struct ps_respack_args {
  const char *srcpath;
  const char *dstpath;
};

static int ps_respack_args_read(struct ps_respack_args *args,int argc,char **argv) {

  /* Verify count of arguments, and for safety's sake make sure they don't begin with a dash.
   */
  if ((argc!=3)||(argv[1][0]=='-')||(argv[2][0]=='-')) {
    ps_log(RESPACK,ERROR,"Usage: %s OUTPUT INPUT",(argc>=1)?argv[0]:"respack");
    return -1;
  }
  
  args->dstpath=argv[1];
  args->srcpath=argv[2];
  return 0;
}

/* Main entry point.
 */
 
int main(int argc,char **argv) {

  struct ps_respack_args args={0};
  if (ps_respack_args_read(&args,argc,argv)<0) {
    ps_log(RESPACK,ERROR,"Malformed request. Aborting.");
    return 1;
  }
  ps_log(RESPACK,TRACE,"Read resources from '%s' and archive to '%s'...",args.srcpath,args.dstpath);
  
  if (ps_resmgr_init(args.srcpath,0)<0) {
    ps_log(RESPACK,ERROR,"Failed to load resources from %s",args.srcpath);
    return 1;
  }
  
  if (ps_res_export_archive(args.dstpath)<0) {
    ps_log(RESPACK,ERROR,"Export failed.");
    return 1;
  }
  
  ps_resmgr_quit();
  return 0;
}
