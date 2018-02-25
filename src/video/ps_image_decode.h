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
