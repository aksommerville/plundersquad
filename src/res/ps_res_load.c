#include "ps_res_internal.h"
#include "os/ps_fs.h"
#include "os/ps_clockassist.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

/* Load one resource from a file.
 * Type and ID are already known from the file's name.
 */

static int ps_resmgr_load_single_file(const char *path,struct ps_restype *type,int rid) {
  ps_log(RES,DEBUG,"Loading resource '%s:%d' from file '%s'...",type->name,rid,path);

  void *src=0;
  int srcc=ps_file_read(&src,path);
  if (srcc<0) {
    ps_log(RES,ERROR,"%s: Failed to read file.",path);
    return -1;
  }

  if (ps_restype_decode(type,rid,src,srcc,path)<0) {
    //ps_log(RES,ERROR,"%s: Failed to decode resource.",path); // Logged better by restype
    free(src);
    return -1;
  }
  
  free(src);
  return 0;
}

/* Resource ID from base name.
 */

static int ps_resmgr_id_from_basename(const char *src,int srcc) {
  int id=0,srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    if (id>INT_MAX/10) return -1;
    id*=10;
    if (id>INT_MAX-digit) return -1;
    id+=digit;
  }
  if (!srcp) return -1;
  if (srcp>=srcc) return id;
  if ((src[srcp]>='a')&&(src[srcp]<='z')) return -1;
  if ((src[srcp]>='A')&&(src[srcp]<='Z')) return -1;
  return id;
}

/* Load resources from directory of known type.
 * Each file must begin with a decimal integer, that is the resource ID.
 */

static int ps_resmgr_load_typed_directory(const char *path,struct ps_restype *type) {
  ps_log(RES,DEBUG,"Loading '%s' resources from directory '%s'...",type->name,path);

  char subpath[1024];
  int pathc=0; while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) {
    ps_log(RES,ERROR,"%s: %s",path,strerror(errno));
    return -1;
  }

  struct dirent *de;
  while (de=readdir(dir)) {
  
    const char *base=de->d_name;
    if (base[0]=='.') continue;
    int basec=0; while (base[basec]) basec++;

    int rid=ps_resmgr_id_from_basename(base,basec);
    if (rid<0) {
      ps_log(RES,WARN,"%s/%s: Ignoring file because name does not begin with resource ID.",path,base);
      continue;
    }

    if (pathc>=sizeof(subpath)-basec) {
      ps_log(RES,ERROR,"File names too long under '%s'.",path);
      closedir(dir);
      return -1;
    }
    memcpy(subpath+pathc,base,basec+1);

    if (ps_resmgr_load_single_file(subpath,type,rid)<0) {
      closedir(dir);
      return -1;
    }
  }

  closedir(dir);
  return 0;
}

/* Load resources from directory.
 * Contents of this directory are directories corresponding to one restype.
 */

static int ps_resmgr_load_directory(const char *path) {
  ps_log(RES,DEBUG,"Loading resources from directory '%s'...",path);

  char subpath[1024];
  int pathc=0; while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) {
    ps_log(RES,ERROR,"%s: %s",path,strerror(errno));
    return -1;
  }

  struct dirent *de;
  while (de=readdir(dir)) {
  
    const char *base=de->d_name;
    if (base[0]=='.') continue;
    int basec=0; while (base[basec]) basec++;
    
    struct ps_restype *type=ps_resmgr_get_type_by_name(base,basec);
    if (!type) {
      ps_log(RES,WARN,"Unexpected file '%s' in resource directory '%s'.",base,path);
      continue;
    }

    if (pathc>=sizeof(subpath)-basec) {
      ps_log(RES,ERROR,"File names too long under '%s'.",path);
      closedir(dir);
      return -1;
    }
    memcpy(subpath+pathc,base,basec+1);

    if (ps_resmgr_load_typed_directory(subpath,type)<0) {
      closedir(dir);
      return -1;
    }
  }

  closedir(dir);
  return 0;
}

/* Load resources from archive file.
 */

static int ps_resmgr_load_file(const char *path) {
  ps_log(RES,ERROR,"TODO: Load resources from archive (%s).",path);
  return -1;
}

/* Reload resources, main entry point.
 */

int ps_resmgr_reload() {
  int64_t starttime=ps_time_now();
  if (!ps_resmgr.init) return -1;

  struct stat st={0};
  if (stat(ps_resmgr.rootpath,&st)<0) {
    ps_log(RES,ERROR,"%.*s: %s",ps_resmgr.rootpathc,ps_resmgr.rootpath,strerror(errno));
    return -1;
  }

  if (ps_resmgr_clear()<0) return -1;

  if (S_ISDIR(st.st_mode)) {
    if (ps_resmgr_load_directory(ps_resmgr.rootpath)<0) return -1;
  } else if (S_ISREG(st.st_mode)) {
    if (ps_resmgr_load_file(ps_resmgr.rootpath)<0) return -1;
  } else {
    ps_log(RES,ERROR,"%.*s: Not a file or directory (mode 0%o)",ps_resmgr.rootpathc,ps_resmgr.rootpath,st.st_mode);
    return -1;
  }

  int64_t elapsed=ps_time_now()-starttime;
  ps_log(RES,INFO,"Loaded resources in %d.%06d s.",(int)(elapsed/1000000),(int)(elapsed%1000000));

  return 0;
}
