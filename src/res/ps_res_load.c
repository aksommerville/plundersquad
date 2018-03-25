#include "ps_res_internal.h"
#include "os/ps_fs.h"
#include "os/ps_clockassist.h"
#include "util/ps_buffer.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

static int ps_resmgr_load_file(const char *path);

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
      if ((basec==5)&&!memcmp(base,"audio",5)) ; // We know about this one, ignore it.
      else ps_log(RES,WARN,"Unexpected file '%s' in resource directory '%s'.",base,path);
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

/* Link all resources.
 */

static int ps_resmgr_link() {
  int i=PS_RESTYPE_COUNT; while (i-->0) {
    if (ps_restype_link(ps_resmgr.typev+i)<0) return -1;
  }
  return 0;
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

  if (ps_resmgr_link()<0) return -1;

  int64_t elapsed=ps_time_now()-starttime;
  ps_log(RES,INFO,"Loaded resources in %d.%06d s.",(int)(elapsed/1000000),(int)(elapsed%1000000));

  return 0;
}

/* Get path for resource -- existing file.
 */

static int ps_res_get_path_for_resource_existing(char *dst,int pfxc,int dsta,int rid) {
  DIR *dir=opendir(dst);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
  
    const char *base=de->d_name;
    if (base[0]=='.') continue;
    int basec=0; while (base[basec]) basec++;

    int qrid=ps_resmgr_id_from_basename(base,basec);
    if (qrid!=rid) continue;

    if (pfxc>dsta-basec) break;
    closedir(dir);
    memcpy(dst+pfxc,base,basec);
    int dstc=pfxc+basec;
    if (dstc<dsta) dst[dstc]=0;
    return dstc;
    
  }
  closedir(dir);
  return -1;
}

/* Get path for resource -- new file.
 */

static int ps_res_get_path_for_resource_generate(char *dst,int pfxc,int dsta,int rid) {
  int dstc=snprintf(dst+pfxc,dsta-pfxc,"%03d",rid);
  if (dstc<0) return -1;
  return pfxc+dstc;
}

/* Get path for resource.
 */
 
int ps_res_get_path_for_resource(char *dst,int dsta,int tid,int rid,int only_if_existing) {
  if (!ps_resmgr.init) return -1;
  if (ps_resmgr.rootpathc<1) return -1;
  if (!dst||(dsta<0)) dsta=0;
  if ((tid<0)||(tid>=PS_RESTYPE_COUNT)) return -1;
  const struct ps_restype *type=ps_resmgr.typev+tid;

  char path[1024];
  int subpathc=snprintf(path,sizeof(path),"%.*s/%s/",ps_resmgr.rootpathc,ps_resmgr.rootpath,type->name);
  if ((subpathc<0)||(subpathc>=sizeof(path))) return -1;

  int dstc=ps_res_get_path_for_resource_existing(path,subpathc,sizeof(path),rid);
  if (dstc>=0) {
    if (dstc>sizeof(path)) return -1;
    if (dstc<=dsta) {
      memcpy(dst,path,dstc);
      if (dstc<dsta) dst[dstc]=0;
    }
    return dstc;
  }

  if (!only_if_existing) {
    dstc=ps_res_get_path_for_resource_generate(path,subpathc,sizeof(path),rid);
    if (dstc>=0) {
      if (dstc>sizeof(path)) return -1;
      if (dstc<=dsta) {
        memcpy(dst,path,dstc);
        if (dstc<dsta) dst[dstc]=0;
      }
      return dstc;
    }
  }
  
  return -1;
}

/* Load resources from archive file.
 */
 
static int ps_resmgr_load_file_inner(struct ps_zlib_file *file,const char *path,struct ps_buffer *buffer) {
  
  uint8_t header[16];
  int headerc=ps_zlib_read(header,16,file);
  if ((headerc!=16)||memcmp(header,"\0PLSQ\xffRA",8)) {
    ps_log(RES,ERROR,"%s: Failed to read resource archive header. headerc=%d",path,headerc);
    return -1;
  }
  
  while (1) {
    headerc=ps_zlib_read(header,5,file);
    if (headerc<0) return -1;
    if (!headerc) break;
    if (headerc!=5) return -1;
    
    int tid=header[0]>>4;
    int rid=((header[0]&0x0f)<<8)|header[1];
    int len=(header[2]<<16)|(header[3]<<8)|header[4];
    
    ps_log(RES,DEBUG,"Decoding from archive. tid=%d rid=%d len=%d",tid,rid,len);
    
    struct ps_restype *restype=ps_resmgr_get_type_by_id(tid);
    if (!restype) {
      ps_log(RES,ERROR,"%s: Invalid resource type ID %d",path,tid);
      return -1;
    }
    
    buffer->c=0;
    if (ps_buffer_require(buffer,len)<0) return -1;
    int err=ps_zlib_read(buffer->v,len,file);
    if (err<0) return -1;
    if (err!=len) return -1;
    
    if (ps_restype_decode(restype,rid,buffer->v,len,path)<0) {
      return -1;
    }
    
  }
  return 0;
}

static int ps_resmgr_load_file(const char *path) {
  struct ps_zlib_file *file=ps_zlib_open(path,0);
  if (!file) return -1;
  struct ps_buffer buffer={0};
  int err=ps_resmgr_load_file_inner(file,path,&buffer);
  ps_buffer_cleanup(&buffer);
  ps_zlib_close(file);
  return err;
}

/* Encode resource for archive export.
 */
 
static int ps_res_encode(void *dst,int dsta,const struct ps_restype *restype,const void *obj) {
  if (!dst||(dsta<0)) dsta=0;
  if (!obj) return -1;
  if (!restype) return -1;
  if (!restype->encode) {
    ps_log(RES,ERROR,"No encoder for '%s' resources.",restype->name);
    return -1;
  }
  return restype->encode(dst,dsta,obj);
}

/* Export single resource into archive.
 */
 
static int ps_res_export_resource(struct ps_zlib_file *file,const struct ps_restype *restype,const struct ps_res *res,struct ps_buffer *buffer) {

  /* Encode resource. */
  buffer->c=0;
  while (1) {
    int err=ps_res_encode(buffer->v,buffer->a,restype,res->obj);
    if (err<0) return -1;
    if (err<=buffer->a) {
      buffer->c=err;
      break;
    }
    if (ps_buffer_require(buffer,err)<0) return -1;
  }
  
  ps_log(RES,DEBUG,"Encoding to archive. tid=%d rid=%d len=%d",restype->tid,res->id,buffer->c);
  
  /* Write resource header. */
  uint8_t header[5]={
    (restype->tid<<4)|(res->id>>8),
    res->id,
    buffer->c>>16,
    buffer->c>>8,
    buffer->c,
  };
  if (ps_zlib_write(file,header,5)<0) return -1;
  
  /* Write raw data. */
  if (ps_zlib_write(file,buffer->v,buffer->c)<0) return -1;

  return 0;
}

/* Export archive into open file.
 * (path) is for reference only.
 */
 
static int ps_res_export_archive_to_zlib_file(struct ps_zlib_file *file,const char *path) {
  
  /* Archive header. */
  if (ps_zlib_write(file,"\0PLSQ\xffRA\0\0\0\0\0\0\0\0",16)<0) return -1;
  
  /* Make a shared buffer so we're not allocating memory so much. */
  struct ps_buffer buffer={0};
  
  /* Resource types. */
  const struct ps_restype *restype=ps_resmgr.typev;
  int tid=0; for (;tid<PS_RESTYPE_COUNT;tid++,restype++) {
    const struct ps_res *res=restype->resv;
    int i=restype->resc; for (;i-->0;res++) {
      if (ps_res_export_resource(file,restype,res,&buffer)<0) {
        ps_log(RES,ERROR,"%s: Failed to export %s:%d",path,restype->name,res->id);
        ps_buffer_cleanup(&buffer);
        return -1;
      }
    }
  }
  
  /* Wrap it up. */
  ps_buffer_cleanup(&buffer);
  if (ps_zlib_write_end(file)<0) return -1;
  
  return 0;
}

/* Export archive.
 */
 
int ps_res_export_archive(const char *path) {
  if (!ps_resmgr.init) return -1;
  if (!path||!path[0]) return -1;
  
  struct ps_zlib_file *file=ps_zlib_open(path,1);
  if (!file) {
    ps_log(RES,ERROR,"%s: Failed to open file for writing, or to initialize the zlib stream.",path);
    return -1;
  }
  
  if (ps_res_export_archive_to_zlib_file(file,path)<0) {
    ps_log(RES,ERROR,"%s: Encoding or writing failed.",path);
    ps_zlib_close(file);
    unlink(path);
    return -1;
  }
  
  ps_zlib_close(file);
  ps_log(RES,INFO,"%s: Exported resource archive.",path);
  return 0;
}
