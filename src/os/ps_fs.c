#include "ps.h"
#include "ps_fs.h"
#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>

/* Read whole file.
 */
 
int ps_file_read(void *dstpp,const char *path) {
  if (!dstpp||!path) return -1;
  #ifdef O_BINARY
    int fd=open(path,O_RDONLY|O_BINARY);
  #else
    int fd=open(path,O_RDONLY);
  #endif
  if (!fd) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write whole file.
 */

int ps_file_write(const char *path,const void *src,int srcc) {
  if (!path||(srcc<0)||(srcc&&!src)) return -1;
  #ifdef O_BINARY
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  #else
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  #endif
  if (fd<0) {
    return -1;
  }
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Zlib archives.
 */
 
#define PS_ZLIB_MODE_UNSET 0
#define PS_ZLIB_MODE_READ  1
#define PS_ZLIB_MODE_WRITE 2
 
struct ps_zlib_file {
  int fd;
  int mode;
  z_stream z;
  int error;
  char *rbuf;
  int rbufp,rbufc,rbufa;
  int rdcomplete;
  int zend;
  char *wbuf;
  int wbufp,wbufc,wbufa;
};

/* Open zlib file.
 */
 
struct ps_zlib_file *ps_zlib_open(const char *path,int output) {
  struct ps_zlib_file *file=calloc(1,sizeof(struct ps_zlib_file));
  if (!file) return 0;
  
  int fmode;
  if (output) {
    fmode=O_WRONLY|O_CREAT|O_TRUNC;
  } else {
    fmode=O_RDONLY;
  }
  #if PS_ARCH==PS_ARCH_MSWIN
    fmode|=O_BINARY;
  #endif
  
  if ((file->fd=open(path,fmode,0666))<0) {
    ps_zlib_close(file);
    return 0;
  }
  
  if (output) {
    if (deflateInit(&file->z,Z_DEFAULT_COMPRESSION)<0) {
      ps_zlib_close(file);
      return 0;
    }
    file->mode=PS_ZLIB_MODE_WRITE;
  } else {
    if (inflateInit(&file->z)<0) {
      ps_zlib_close(file);
      return 0;
    }
    file->mode=PS_ZLIB_MODE_READ;
  }
  
  return file;
}

/* Close zlib file.
 */
 
void ps_zlib_close(struct ps_zlib_file *file) {
  if (!file) return;
  switch (file->mode) {
    case PS_ZLIB_MODE_READ: inflateEnd(&file->z); break;
    case PS_ZLIB_MODE_WRITE: deflateEnd(&file->z); break;
  }
  if (file->fd>=0) close(file->fd);
  if (file->rbuf) free(file->rbuf);
  if (file->wbuf) free(file->wbuf);
  free(file);
}

/* Refill read buffer.
 */
 
static int ps_zlib_refill_rbuf(struct ps_zlib_file *file) {
  if (file->rdcomplete) return 0;
  
  if (file->rbufp+file->rbufc>=file->rbufa) {
    if (file->rbufp) {
      memmove(file->rbuf,file->rbuf+file->rbufp,file->rbufc);
      file->rbufp=0;
    } else {
      if (file->rbufa>INT_MAX-8192) return -1;
      int na=file->rbufa+8192;
      void *nv=realloc(file->rbuf,na);
      if (!nv) return -1;
      file->rbuf=nv;
      file->rbufa=na;
    }
  }
  
  int err=read(file->fd,file->rbuf+file->rbufp+file->rbufc,file->rbufa-file->rbufc-file->rbufp);
  if (err<0) return -1;
  if (!err) {
    file->rdcomplete=1;
    return 0;
  }
  file->rbufc+=err;
  
  return 0;
}

/* Read from zlib file.
 */
 
int ps_zlib_read(void *dst,int dsta,struct ps_zlib_file *file) {
  if (!dst||(dsta<1)||!file) return -1;
  if (file->mode!=PS_ZLIB_MODE_READ) return -1;
  if (file->zend) return 0;
  
  file->z.next_out=(Bytef*)dst;
  file->z.avail_out=dsta;
  while (file->z.avail_out) {
  
    if (ps_zlib_refill_rbuf(file)<0) return -1;
    if (file->rbufc<1) break;
    file->z.next_in=(Bytef*)file->rbuf+file->rbufp;
    file->z.avail_in=file->rbufc;
    
    int err=inflate(&file->z,Z_NO_FLUSH);
    if (err<0) {
      err=inflate(&file->z,Z_SYNC_FLUSH);
      if (err<0) {
        file->error=err;
        return -1;
      }
    }
    
    int rbufconsumec=file->rbufc-file->z.avail_in;
    if ((file->rbufc-=rbufconsumec)<=0) {
      file->rbufp=file->rbufc=0;
    } else {
      file->rbufp+=rbufconsumec;
    }
    
    if (err==Z_STREAM_END) {
      file->zend=1;
      break;
    }
  }
  
  int dstc=dsta-file->z.avail_out;
  file->z.next_out=0;
  file->z.avail_out=0;
  return dstc;
}

/* Flush output buffer to file.
 */
 
static int ps_zlib_wbuf_write(struct ps_zlib_file *file) {
  while (file->wbufc>0) {
    int err=write(file->fd,file->wbuf+file->wbufp,file->wbufc);
    if (err<=0) return -1;
    file->wbufp+=err;
    file->wbufc-=err;
  }
  file->wbufp=0;
  return 0;
}

/* Require output buffer.
 */
 
static int ps_zlib_wbuf_require(struct ps_zlib_file *file) {
  if (file->wbufp+file->wbufc<file->wbufa) return 0;
  if (file->wbufp) {
    memmove(file->wbuf,file->wbuf+file->wbufp,file->wbufc);
    file->wbufp=0;
    return 0;
  }
  if (file->wbufa>INT_MAX-8192) return -1;
  int na=file->wbufa+8192;
  void *nv=realloc(file->wbuf,na);
  if (!nv) return -1;
  file->wbuf=nv;
  file->wbufa=na;
  return 0;
}

/* Write to zlib file.
 */
 
int ps_zlib_write(struct ps_zlib_file *file,const void *src,int srcc) {
  if (!file||!src||(srcc<0)) return -1;
  if (file->mode!=PS_ZLIB_MODE_WRITE) return -1;
  if (file->fd<0) return -1;
  
  file->z.next_in=(Bytef*)src;
  file->z.avail_in=srcc;
  while (file->z.avail_in) {
  
    if (ps_zlib_wbuf_require(file)<0) return -1;
    file->z.next_out=(Bytef*)file->wbuf+file->wbufp+file->wbufc;
    file->z.avail_out=file->wbufa-file->wbufc-file->wbufp;
    int avail_out0=file->z.avail_out;
    
    int err=deflate(&file->z,Z_NO_FLUSH);
    if (err<0) {
      err=deflate(&file->z,Z_SYNC_FLUSH);
      if (err<0) {
        file->error=err;
        return -1;
      }
    }
    
    int addc=avail_out0-file->z.avail_out;
    file->wbufc+=addc;
  
  }
  
  file->z.next_in=0;
  file->z.avail_in=0;
  return srcc;
}

/* Finish writing zlib file.
 */
 
int ps_zlib_write_end(struct ps_zlib_file *file) {
  if (!file) return -1;
  if (file->mode!=PS_ZLIB_MODE_WRITE) return -1;
  if (file->fd<0) return -1;
  
  file->z.next_in=0;
  file->z.avail_in=0;
  while (1) {
  
    if (ps_zlib_wbuf_require(file)<0) return -1;
    file->z.next_out=(Bytef*)file->wbuf+file->wbufp+file->wbufc;
    file->z.avail_out=file->wbufa-file->wbufc-file->wbufp;
    int avail_out0=file->z.avail_out;
    
    int err=deflate(&file->z,Z_FINISH);
    if (err<0) {
      file->error=err;
      return -1;
    }
    
    int addc=avail_out0-file->z.avail_out;
    file->wbufc+=addc;
    if (err==Z_STREAM_END) break;
  }
  
  if (ps_zlib_wbuf_write(file)<0) return -1;
  
  close(file->fd);
  file->fd=-1;
  
  return 0;
}
