/* ps_buffer.h
 * General-purpose mutable string.
 */

#ifndef PS_BUFFER_H
#define PS_BUFFER_H

struct ps_buffer {
  char *v;
  int c,a;
};

void ps_buffer_cleanup(struct ps_buffer *buffer);

int ps_buffer_require(struct ps_buffer *buffer,int addc);

int ps_buffer_replace(struct ps_buffer *buffer,int p,int c,const void *src,int srcc);
int ps_buffer_append(struct ps_buffer *buffer,const void *src,int srcc);
int ps_buffer_clear(struct ps_buffer *buffer);

int ps_buffer_appendf(struct ps_buffer *buffer,const char *fmt,...);
int ps_buffer_appendfv(struct ps_buffer *buffer,const char *fmt,va_list vargs);

int ps_buffer_append_be8(struct ps_buffer *buffer,uint8_t src);
int ps_buffer_append_be16(struct ps_buffer *buffer,uint16_t src);
int ps_buffer_append_be32(struct ps_buffer *buffer,uint32_t src);

int ps_buffer_terminate(struct ps_buffer *buffer);

#endif
