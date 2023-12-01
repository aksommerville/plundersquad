/* ps_alsa.h
 * Shallow interface to ALSA.
 * Provides an I/O thread that hits your callback with a buffer ready for delivery.
 * And that's all.
 */

#ifndef PS_ALSA_H
#define PS_ALSA_H

#include <stdint.h>

int ps_alsa_init(const char *device,int rate,int chanc,void (*cb)(int16_t *dst,int dstc));
void ps_alsa_quit();
int ps_alsa_lock();
int ps_alsa_unlock();

#endif
