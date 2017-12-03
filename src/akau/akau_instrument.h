/* akau_instrument.h
 * Defines an object containing a floating-point wave with configurable attack and decay timing.
 *
 * Attack time is the count of frames from start to peak level (attack trim).
 * Drawback time is the count of frames from peak level to sustained level (drawback trim).
 * Decay time is the count of frames leading out to silence.
 * These produce output like this:
 *          /\
 *         /  \______________________
 *        /                          \
 *       /                            \
 *   ___/                              \___
 *
 * Probably you will want all instruments to have the same attack and drawback times.
 * Decay time is very nice for producing a sustain effect.
 *
 * It is unadvisable to use zero for attack or decay time, as this might cause popping artifacts.
 *
 * Instruments are constant after construction.
 */

#ifndef AKAU_INSTRUMENT_H
#define AKAU_INSTRUMENT_H

struct akau_instrument;
struct akau_fpcm;
struct akau_store;

/* Create a new instrument.
 * 'time' arguments are in frames.
 * 'trim' arguments are in 0..1. (greater is allowed but probably a bad idea).
 * (fpcm) is required, and we will keep it locked.
 */
struct akau_instrument *akau_instrument_new(
  struct akau_fpcm *fpcm,
  int attack_time,
  double attack_trim,
  int drawback_time,
  double drawback_trim,
  int decay_time
);

/* Create a new instrument with fpcm to be filled in later.
 */
struct akau_instrument *akau_instrument_new_deferred(
  int fpcmid,
  int attack_time,
  double attack_trim,
  int drawback_time,
  double drawback_trim,
  int decay_time
);

/* Create an instrument with default timings.
 * This is used when an instrument is stored as a PCM format.
 */
struct akau_instrument *akau_instrument_new_default(
  struct akau_fpcm *fpcm
);

void akau_instrument_del(struct akau_instrument *instrument);
int akau_instrument_ref(struct akau_instrument *instrument);

struct akau_fpcm *akau_instrument_get_fpcm(const struct akau_instrument *instrument);
int akau_instrument_get_attack_time(const struct akau_instrument *instrument);
int akau_instrument_get_drawback_time(const struct akau_instrument *instrument);
int akau_instrument_get_decay_time(const struct akau_instrument *instrument);
double akau_instrument_get_attack_trim(const struct akau_instrument *instrument);
double akau_instrument_get_drawback_trim(const struct akau_instrument *instrument);

/* Produce a new instrument by decoding text.
 * Format is: ( ATTACKTIME ATTACKTRIM DRAWBACKTIME DRAWBACKTRIM DECAYTIME ) FPCM
 *   TIME values are decimal integers in milliseconds (not frames!).
 *   TRIM values are decimal integers 0..255.
 *   FPCM is either:
 *     "external RESID"
 *     or delivered to akau_decode_fpcm_harmonics
 */
struct akau_instrument *akau_instrument_decode(const char *src,int srcc);

/* Newer alternative to akau_instrument_decode().
 * Instruments can store as a binary list of timings, levels, and coefficients.
 *   0000   2 Attack time, ms.
 *   0002   2 Drawback time, ms.
 *   0004   2 Decay time, ms.
 *   0006   1 Attack trim.
 *   0007   1 Drawback trim.
 *   0008   1 unused.
 *   0009   1 Coefficient count.
 *   000a ... Coefficients, 2 bytes each, 0xffff => 1.0.
 * Note that there are no "encode" functions.
 * Producing an instrument is a one-way trip.
 * You should store the original encoded form elsewhere if you might need it.
 */
struct akau_instrument *akau_instrument_decode_binary(const void *src,int srcc);
int akau_instrument_text_from_binary(char *dst,int dsta,const void *src,int srcc);
int akau_instrument_binary_from_text(void *dst,int dsta,const char *src,int srcc);

/* If this instrument was decoded with an "external" reference, find the fpcm and bind them.
 */
int akau_instrument_link(struct akau_instrument *instrument,struct akau_store *store);

#endif
