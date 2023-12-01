/* akau_driver.h
 * Abstraction of akau-compatible backend.
 * Since akau is pure portable software, it does not claim any knowledge of these.
 */

#ifndef AKAU_DRIVER_H
#define AKAU_DRIVER_H

#include <stdint.h>

/* Fill (dst) with raw PCM, even slots are left and odd are right.
 * (dstc) is the count of samples to provide (not bytes, not frames).
 */
typedef void (*akau_cb_fn)(int16_t *dst,int dstc);

struct akau_driver {
  int (*init)(const char *device,int rate,int chanc,akau_cb_fn cb);
  void (*quit)();
  int (*lock)();
  int (*unlock)();
};

/* These drivers are defined in their respective units.
 * You must include that unit, or linkage will fail.
 */
extern const struct akau_driver akau_driver_akmacaudio;
extern const struct akau_driver akau_driver_alsa;
extern const struct akau_driver akau_driver_msaudio;

#endif
