/* akpng.h
 * PNG codec.
 * Encoding and decoding is all in one pass.
 * We do not support interlaced images. Maybe I'll revisit that decision later, probably not.
 * We support image conversion into an opinionated set of 3 formats (RGB,RGBA,Y8).
 *
 * Typical usage, read PNG from a file and require RGBA:
 *   struct akpng_image image={0};
 *   if (
 *     (akpng_decode_file(&image,path_to_my_file)<0)||
 *     (akpng_image_force_rgb(&image,1)<0)
 *   ) {
 *     akpng_image_cleanup(&image);
 *     return -1;
 *   }
 *   // Do something with the image.
 *   akpng_image_cleanup(&image);
 *
 * Link: -lz
 */

#ifndef AKPNG_H
#define AKPNG_H

#include <stdint.h>

struct akpng_chunk {
  char type[4];
  int c;
  void *v;
};

struct akpng_image {
  void *pixels;
  int w,h;
  int depth;
  int colortype;
  int stride; // Can calculate from (w,colortype,depth), but we provide for convenience.
  struct akpng_chunk *chunkv; // (IHDR,IDAT,IEND) not included.
  int chunkc,chunka;
};

/* Free all dynamic data and wipe structure.
 */
void akpng_image_cleanup(struct akpng_image *image);

/* Wipe the image and reallocate for the given dimensions.
 * Extra chunks will be destroyed.
 * (image) is untouched if reallocation is not possible.
 */
int akpng_image_realloc(struct akpng_image *image,int w,int h,int depth,int colortype);

/* Decode from a complete stream in memory or from a file.
 * Decoding from file is only for convenience; it is no more memory-efficient.
 * Any prior content in (image) is safely wiped.
 * Likewise, we wipe the image before returning any error.
 */
int akpng_decode(struct akpng_image *image,const void *src,int srcc);
int akpng_decode_file(struct akpng_image *image,const char *path);

/* Encode an image to memory or directly to a file.
 * We don't enforce any chunk rules, so it is possible to produce an invalid file here.
 */
int akpng_encode(void *dstpp,const struct akpng_image *image);
int akpng_encode_file(const char *path,const struct akpng_image *image);

/* Create a new chunk in (image), at the end of its list.
 * (image) assumes ownership of the buffer in "_handoff", for the others it copies input.
 */
int akpng_image_copy_chunk(struct akpng_image *image,const struct akpng_chunk *chunk);
int akpng_image_create_chunk(struct akpng_image *image,const char *type,const void *v,int c);
int akpng_image_create_chunk_handoff(struct akpng_image *image,const char *type,void *v,int c);

/* If a chunk exists, return it (a pointer directly into the image's list).
 * Multiple chunks of the same type begin with index 0, then 1, etc.
 * IHDR, IEND, and IDAT are not retrievable.
 */
struct akpng_chunk *akpng_image_get_chunk(const struct akpng_image *image,const char *type,int index);

/* Convert an image to 24-bit RGB, 32-bit RGBA, or 8-bit grayscale.
 * We do not support general conversion, only these three destination formats.
 * To RGB, you may indicate -1 to drop alpha, 0 to preserve if present, or 1 to force RGBA.
 * tRNS and PLTE chunks are modified as necessary; all other chunks are preserved verbatim.
 */
int akpng_image_force_rgb(struct akpng_image *image,int alpha);
int akpng_image_force_gray(struct akpng_image *image);

/* Nonzero if (image) contains an alpha channel or a valid tRNS chunk.
 */
int akpng_image_has_transparency(const struct akpng_image *image);

#endif
