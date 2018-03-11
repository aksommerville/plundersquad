/* ps_image_decode.h
 * Decode an image file into an akgl texture.
 */

#ifndef PS_IMAGE_DECODE_H
#define PS_IMAGE_DECODE_H

struct akgl_texture;

/* (src) contains an entire image file.
 * (refpath) is the optional file name for logging.
 * Video backend must be initialized first.
 * We return a new texture object on success, or NULL on error.
 * We accept three formats:
 *   - PNG (see <akpng/akpng.h>)
 *   - psimage (see below)
 *   - Raw pixels, must be the precise size expected of a tilesheet.
 */
struct akgl_texture *ps_image_decode(
  const void *src,int srcc,
  const char *refpath
);

/* Same idea, but we stop at the raw image decode.
 * This should only be needed by tests.
 * Since it's not "production" code, it's incomplete. We only support PNG and don't log extensively.
 */
int ps_image_decode_pixels(
  void *pixelspp,int *fmt,int *w,int *h,
  const void *src,int srcc
);

/* Encode to psimage format.
 */
int ps_image_encode(void *dst,int dsta,const void *pixels,int w,int h,int fmt);

/* Pixel size in bytes for the given format, 0 if invalid.
 */
int ps_pixelsize_for_akgl_fmt(int fmt);

/* psimage format uses filtering inspired by PNG, but not as involved.
 * There is no leading byte, so input and output are the same size.
 * Every row has PNG's "SUB" filter, subtracting the corresponding value from the previous row.
 * ...I've tried PAETH too and still filtering ends up increasing the total size.
 * These are reverted to no-op.
 */
void ps_image_filter(void *dst,const void *src,int colstride,int rowstride,int h);
void ps_image_unfilter(void *dst,const void *src,int colstride,int rowstride,int h);

/* === psimage format ===
 * Begins with 16-byte header:
 *   0000   8 Signature: "PSIMG\0\xff\0"
 *   0008   2 Width
 *   000a   2 Height
 *   000c   1 Format (AKGL_FMT_*)
 *   000d   3 Reserved
 *   0010
 * Followed by packed pixels.
 * Since every AKGL format is minimum 8 bits per pixel, rows are naturally byte-aligned.
 */

#endif
