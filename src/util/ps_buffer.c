#include "ps.h"
#include "ps_buffer.h"
#include <zlib.h>

/* Cleanup.
 */

void ps_buffer_cleanup(struct ps_buffer *buffer) {
  if (!buffer) return;
  if (buffer->v) free(buffer->v);
  memset(buffer,0,sizeof(struct ps_buffer));
}

/* Realloate.
 */

int ps_buffer_require(struct ps_buffer *buffer,int addc) {
  if (!buffer) return -1;
  if (addc<1) return 0;
  if (buffer->c>INT_MAX-addc) return -1;
  int na=buffer->c+addc;
  if (na<=buffer->a) return 0;
  if (na<INT_MAX-256) na=(na+256)&~255;
  void *nv=realloc(buffer->v,na);
  if (!nv) return -1;
  buffer->v=nv;
  buffer->a=na;
  return 0;
}

/* Replace.
 */

int ps_buffer_replace(struct ps_buffer *buffer,int p,int c,const void *src,int srcc) {
  if (!buffer) return -1;
  if ((p<0)||(p>buffer->c)) p=buffer->c;
  if ((c<0)||(p>buffer->c-c)) c=buffer->c-p;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (ps_buffer_require(buffer,srcc-c)<0) return -1;
  memmove(buffer->v+p+srcc,buffer->v+p+c,buffer->c-c-p);
  memcpy(buffer->v+p,src,srcc);
  buffer->c+=srcc-c;
  return 0;
}

/* Append.
 */
 
int ps_buffer_append(struct ps_buffer *buffer,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (ps_buffer_require(buffer,srcc)<0) return -1;
  memcpy(buffer->v+buffer->c,src,srcc);
  buffer->c+=srcc;
  return 0;
}

/* Clear.
 */
 
int ps_buffer_clear(struct ps_buffer *buffer) {
  if (!buffer) return -1;
  buffer->c=0;
  return 0;
}

/* Append formatted string.
 */

int ps_buffer_appendf(struct ps_buffer *buffer,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return ps_buffer_appendfv(buffer,fmt,vargs);
}

int ps_buffer_appendfv(struct ps_buffer *buffer,const char *fmt,va_list vargs) {
  if (!buffer||!fmt) return -1;
  while (1) {
    va_list _vargs;
    va_copy(_vargs,vargs);
    int addc=vsnprintf(buffer->v+buffer->c,buffer->a-buffer->c,fmt,_vargs);
    if ((addc<0)||(addc==INT_MAX)) return -1;
    if (buffer->c<buffer->a-addc) { // sic '<' not '<='
      buffer->c+=addc;
      va_copy(vargs,_vargs);
      return 0;
    }
    if (ps_buffer_require(buffer,addc+1)<0) return -1;
  }
}

/* Append big-endian integers.
 */
 
int ps_buffer_append_be8(struct ps_buffer *buffer,uint8_t src) {
  return ps_buffer_append(buffer,&src,1);
}

int ps_buffer_append_be16(struct ps_buffer *buffer,uint16_t src) {
  uint8_t tmp[2]={src>>8,src};
  return ps_buffer_append(buffer,tmp,2);
}

int ps_buffer_append_be32(struct ps_buffer *buffer,uint32_t src) {
  uint8_t tmp[4]={src>>24,src>>16,src>>8,src};
  return ps_buffer_append(buffer,tmp,4);
}

/* Append after compressing with zlib.
 */
 
int ps_buffer_compress_and_append(struct ps_buffer *buffer,const void *src,int srcc) {
  if (!buffer||(srcc<0)||(srcc&&!src)) return -1;
  z_stream z={0};
  if (deflateInit(&z,Z_BEST_COMPRESSION)<0) return -1;

  int reqc=deflateBound(&z,srcc);
  if (reqc>8192) reqc=8192; // Sanity limit; it's no problem for us to do multiple passes.

  z.next_in=(Bytef*)src;
  z.avail_in=srcc;
  int zend=0,mode=Z_NO_FLUSH;
  while (1) {
    if (!z.avail_in) mode=Z_FINISH;
    if (ps_buffer_require(buffer,reqc)<0) {
      deflateEnd(&z);
      return -1;
    }
    z.next_out=(Bytef*)(buffer->v+buffer->c);
    z.avail_out=buffer->a-buffer->c;
    int avail_out_0=z.avail_out;
    int err=deflate(&z,mode);
    if (err<0) {
      deflateEnd(&z);
      return -1;
    }
    buffer->c+=avail_out_0-z.avail_out;
    if (err==Z_STREAM_END) {
      zend=1;
      break;
    }
  }

  deflateEnd(&z);
  return 0;
}

/* Terminate.
 */
 
int ps_buffer_terminate(struct ps_buffer *buffer) {
  if (ps_buffer_require(buffer,1)<0) return -1;
  buffer->v[buffer->c]=0;
  return 0;
}
