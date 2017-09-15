#include "akpng_internal.h"
#include <fcntl.h>
#include <unistd.h>

/* The Paeth predictor.
 */

static inline uint8_t akpng_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=(p>a)?(p-a):(a-p);
  int pb=(p>b)?(p-b):(b-p);
  int pc=(p>c)?(p-c):(c-p);
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

/* Perform a row filter. (encoding)
 */

int akpng_perform_filter(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride,int filter) {
  switch (filter) {
    case 0: memcpy(dst,src,stride); return 0;
    case 1: {
        memcpy(dst,src,xstride);
        int i=xstride; for (;i<stride;i++) {
          dst[i]=src[i]-src[i-xstride];
        }
      } return 0;
    case 2: {
        if (prv) {
          int i=0; for (;i<stride;i++) {
            dst[i]=src[i]-prv[i];
          }
        } else {
          memcpy(dst,src,stride);
        }
      } return 0;
    case 3: {
        if (prv) {
          int i=0; for (;i<xstride;i++) {
            dst[i]=src[i]-(prv[i]>>1);
          }
          for (;i<stride;i++) {
            dst[i]=src[i]-((src[i-xstride]+prv[i])>>1);
          }
        } else {
          memcpy(dst,src,xstride);
          int i=xstride;
          for (;i<stride;i++) {
            dst[i]=src[i]-(src[i-xstride]>>1);
          }
        }
      } return 0;
    case 4: {
        int i=0; for (;i<stride;i++) {
          uint8_t a=(i>=xstride)?src[i-xstride]:0;
          uint8_t b=prv?prv[i]:0;
          uint8_t c=((i>=xstride)&&prv)?prv[i-xstride]:0;
          dst[i]=src[i]-akpng_paeth(a,b,c);
        }
      } return 0;
    default: return -1;
  }
}

/* Undo a row filter. (decoding)
 */

int akpng_perform_unfilter(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride,int filter) {
  switch (filter) {
    case 0: memcpy(dst,src,stride); return 0;
    case 1: {
        memcpy(dst,src,xstride);
        int i=xstride; for (;i<stride;i++) {
          dst[i]=src[i]+dst[i-xstride];
        }
      } return 0;
    case 2: {
        if (prv) {
          int i=0; for (;i<stride;i++) {
            dst[i]=src[i]+prv[i];
          }
        } else {
          memcpy(dst,src,stride);
        }
      } return 0;
    case 3: {
        if (prv) {
          int i=0; for (;i<xstride;i++) {
            dst[i]=src[i]+(prv[i]>>1);
          }
          for (;i<stride;i++) {
            dst[i]=src[i]+((dst[i-xstride]+prv[i])>>1);
          }
        } else {
          memcpy(dst,src,xstride);
          int i=xstride;
          for (;i<stride;i++) {
            dst[i]=src[i]+(dst[i-xstride]>>1);
          }
        }
      } return 0;
    case 4: {
        int i=0; for (;i<stride;i++) {
          uint8_t a=(i>=xstride)?dst[i-xstride]:0;
          uint8_t b=prv?prv[i]:0;
          uint8_t c=((i>=xstride)&&prv)?prv[i-xstride]:0;
          dst[i]=src[i]+akpng_paeth(a,b,c);
        }
      } return 0;
    default: return -1;
  }
}

/* Decode from file.
 */

int akpng_decode_file(struct akpng_image *image,const char *path) {
  if (!image||!path) return -1;

  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *src=malloc(flen);
  if (!src) { close(fd); return -1; }
  int srcc=0; while (srcc<flen) {
    int err=read(fd,src+srcc,flen-srcc);
    if (err<=0) {
      close(fd);
      free(src);
      return -1;
    }
    srcc+=err;
  }
  close(fd);

  int err=akpng_decode(image,src,srcc);
  free(src);
  return err;
}

/* Encode to file.
 */

int akpng_encode_file(const char *path,const struct akpng_image *image) {
  if (!path) return -1;
  void *src=0;
  int srcc=akpng_encode(&src,image);
  if ((srcc<0)||!src) return -1;
  int fd=open(path,O_WRONLY|O_CREAT,0666);
  if (fd<0) { free(src); return -1; }
  int srcp=0; while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      free(src);
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  free(src);
  return 0;
}
