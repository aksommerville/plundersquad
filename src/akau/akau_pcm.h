/* akau_pcm.h
 * Raw wave storage.
 * We define two types: ipcm and fpcm.
 * ipcm uses signed 16-bit integers and fpcm uses double.
 * Generally, something that needs heavy mixing should use fpcm, 
 * and something ready for output should use ipcm.
 * The size of a PCM object is fixed at construction and is never zero.
 */

#ifndef AKAU_PCM_H
#define AKAU_PCM_H

#include <stdint.h>

struct akau_ipcm;
struct akau_fpcm;

/* Make a new ipcm from existing fpcm.
 * (normal) is the output level of input's average level by RMS.
 */
struct akau_ipcm *akau_ipcm_from_fpcm(const struct akau_fpcm *fpcm,int16_t normal);

/* Make a new fpcm from existing ipcm.
 * (normal) is the input level that produces 1.0 in output.
 */
struct akau_fpcm *akau_fpcm_from_ipcm(const struct akau_ipcm *ipcm,int16_t normal);

/* Simple copy.
 */
struct akau_ipcm *akau_ipcm_copy(const struct akau_ipcm *ipcm);
struct akau_fpcm *akau_fpcm_copy(const struct akau_fpcm *fpcm);

/* Size is established at construction and never changes.
 * Objects use simple reference counting.
 */
struct akau_ipcm *akau_ipcm_new(int samplec);
void akau_ipcm_del(struct akau_ipcm *ipcm);
int akau_ipcm_ref(struct akau_ipcm *ipcm);

/* While locked, all mutator functions will automatically fail.
 * Each lock must be matched by one unlock.
 */
int akau_ipcm_lock(struct akau_ipcm *ipcm);
int akau_ipcm_unlock(struct akau_ipcm *ipcm);

/* Trivial accessors.
 * 'has_loop' and 'is_locked' are boolean: zero or one.
 * 'loop_start' is inclusive; 'loop_end' is exclusive.
 * If there is no loop, both loop endpoints report as -1.
 */
int akau_ipcm_get_sample_count(const struct akau_ipcm *ipcm);
int akau_ipcm_has_loop(const struct akau_ipcm *ipcm);
int akau_ipcm_get_loop_start(const struct akau_ipcm *ipcm);
int akau_ipcm_get_loop_end(const struct akau_ipcm *ipcm);
int akau_ipcm_is_locked(const struct akau_ipcm *ipcm);

/* Get a single sample, zero if out of range.
 * The fpcm equivalent uses 0.0 for out of range.
 */
int16_t akau_ipcm_get_sample(const struct akau_ipcm *ipcm,int p);

/* Return the next position after (p), usually (p+addp).
 * Return -1 if invalid or complete.
 * With (loop) nonzero, respect loop points if present.
 */
int akau_ipcm_advance(struct akau_ipcm *ipcm,int p,int addp,int loop);

/* Mutate samples or metadata.
 * These only work on unlocked objects.
 * 'clear' clears samples but not loop points.
 * 'clip' returns the count of samples clipped.
 * Amplifying ipcm may cause clipping, this is not true of fpcm.
 */
int akau_ipcm_set_sample(struct akau_ipcm *ipcm,int p,int16_t sample);
int akau_ipcm_set_loop(struct akau_ipcm *ipcm,int start,int end);
int akau_ipcm_unset_loop(struct akau_ipcm *ipcm);
int akau_ipcm_clear(struct akau_ipcm *ipcm);
int akau_ipcm_amplify(struct akau_ipcm *ipcm,double multiplier);
int akau_ipcm_clip(struct akau_ipcm *ipcm,int16_t threshold);

/* Attenuate level linearly at front or back of wave.
 */
int akau_ipcm_fade_in(struct akau_ipcm *ipcm,int samplec);
int akau_ipcm_fade_out(struct akau_ipcm *ipcm,int samplec);

/* Randomize existing samples in place in a controlled fashion.
 * (amount) is (0..1), the desired difference from input.
 * If (relative) nonzero, we multiply by existing amplitude, roughly preserving the shape.
 * If (relative) zero, we are basically mixing against white noise.
 */
int akau_ipcm_noisify(struct akau_ipcm *ipcm,double amount,int relative);

/* Basic wave analysis.
 * 'rms' is the average level by the Root-Mean-Squared formula.
 * 'peak' is the highest absolute value of any sample.
 */
int16_t akau_ipcm_calculate_rms(const struct akau_ipcm *ipcm);
int16_t akau_ipcm_calculate_peak(const struct akau_ipcm *ipcm);

/* Return a direct pointer to the object's sample buffer, for performance-sensitive applications.
 * USE WITH CAUTION. It is up to you to respect the lock flag and buffer length.
 */
int16_t *akau_ipcm_get_sample_buffer(struct akau_ipcm *ipcm);

/* Decode serialized PCM data.
 * Input may be Microsoft WAV or our own "AKAUPCM" format.
 */
struct akau_ipcm *akau_ipcm_decode(const void *src,int srcc);
struct akau_fpcm *akau_fpcm_decode(const void *src,int srcc);

/******************************************************************************
 * SERIAL FORMATS
 *-----------------------------------------------------------------------------
 * ===== Microsoft WAV =====
 * The most common forms of Microsoft WAV are accepted.
 * Many legal but unusual forms of it will be rejected.
 * I won't go into the fine details here.
 *-----------------------------------------------------------------------------
 * ===== AKAU PCM =====
 * Raw monaural PCM with a brief header:
 *   0000   8 Signature: "\0AKAUPCM"
 *   0008   4 Sample rate, Hz.
 *   000c   4 Loop start position.
 *   0010   4 Loop end position.
 *   0014   4 Format:
 *             1 Signed 16-bit little-endian.
 *             2 Signed 16-bit big-endian.
 *   0018 ... Data.
 *****************************************************************************/

/* fpcm has exactly the same interface as ipcm, but samples are 'double' instead of 'int16_t'.
 * In most cases, an 'fpcm' function is just an alias to an 'ipcm' function.
 */

struct akau_fpcm *akau_fpcm_new(int samplec);
double akau_fpcm_get_sample(const struct akau_fpcm *fpcm,int p);
int akau_fpcm_clear(struct akau_fpcm *fpcm);
int akau_fpcm_amplify(struct akau_fpcm *fpcm,double multiplier);
int akau_fpcm_clip(struct akau_fpcm *fpcm,double threshold);
int akau_fpcm_set_sample(struct akau_fpcm *fpcm,int p,double sample);
double *akau_fpcm_get_sample_buffer(struct akau_fpcm *fpcm);
double akau_fpcm_calculate_rms(const struct akau_fpcm *fpcm);
double akau_fpcm_calculate_peak(const struct akau_fpcm *fpcm);
int akau_fpcm_fade_in(struct akau_fpcm *fpcm,int samplec);
int akau_fpcm_fade_out(struct akau_fpcm *fpcm,int samplec);
int akau_fpcm_noisify(struct akau_fpcm *fpcm,double amount,int relative);

static inline void akau_fpcm_del(struct akau_fpcm *fpcm) { akau_ipcm_del((struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_ref(struct akau_fpcm *fpcm) { return akau_ipcm_ref((struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_lock(struct akau_fpcm *fpcm) { return akau_ipcm_lock((struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_unlock(struct akau_fpcm *fpcm) { return akau_ipcm_unlock((struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_get_sample_count(const struct akau_fpcm *fpcm) { return akau_ipcm_get_sample_count((const struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_has_loop(const struct akau_fpcm *fpcm) { return akau_ipcm_has_loop((const struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_get_loop_start(const struct akau_fpcm *fpcm) { return akau_ipcm_get_loop_start((const struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_get_loop_end(const struct akau_fpcm *fpcm) { return akau_ipcm_get_loop_end((const struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_is_locked(const struct akau_fpcm *fpcm) { return akau_ipcm_is_locked((const struct akau_ipcm*)fpcm); }
static inline int akau_fpcm_advance(struct akau_fpcm *fpcm,int p,int addp,int loop) { return akau_ipcm_advance((struct akau_ipcm*)fpcm,p,addp,loop); }
static inline int akau_fpcm_set_loop(struct akau_fpcm *fpcm,int start,int end) { return akau_ipcm_set_loop((struct akau_ipcm*)fpcm,start,end); }
static inline int akau_fpcm_unset_loop(struct akau_fpcm *fpcm) { return akau_ipcm_unset_loop((struct akau_ipcm*)fpcm); }

#endif
