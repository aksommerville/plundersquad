/* akau_wavegen.h
 * Synthesizer bits, to generate small PCM waves.
 */

#ifndef AKAU_WAVEGEN_H
#define AKAU_WAVEGEN_H

struct akau_ipcm;
struct akau_fpcm;
struct akau_wavegen_decoder;

/* We keep one global variable: A simple normalized sine wave at 44100 Hz.
 * This is generated the first time anyone asks for it.
 */
struct akau_fpcm *akau_wavegen_get_shared_sine();
void akau_wavegen_cleanup_shared_sine();

/* Generate a single iteration of a primitive wave, to ipcm or fpcm.
 * Output levels are always normalized to (-1..1) for fpcm and (-32768..32767) for ipcm.
 * Sine and square take a (balance) parameter to produce warped waves, which is probably not useful.
 * (balance) is the position of the middle zero crossing. Zero means no adjustment.
 */
struct akau_fpcm *akau_generate_fpcm_sine(int samplec,int8_t balance);
struct akau_fpcm *akau_generate_fpcm_square(int samplec,int8_t balance);
struct akau_fpcm *akau_generate_fpcm_saw(int samplec);
struct akau_ipcm *akau_generate_ipcm_sine(int samplec,int8_t balance);
struct akau_ipcm *akau_generate_ipcm_square(int samplec,int8_t balance);
struct akau_ipcm *akau_generate_ipcm_saw(int samplec);

/* Generate a "wave" of random samples.
 * This depends on rand(), obviously.
 */
struct akau_fpcm *akau_generate_fpcm_whitenoise(int samplec);
struct akau_ipcm *akau_generate_ipcm_whitenoise(int samplec);

/* Generate a single iteration of a wave by combining harmonics.
 * Output wave will be the same size as (reference).
 * If you do not provide a reference wave, we use the shared sine; this is usually what you want.
 * If (normalize) nonzero, coefficients will be divided by their harmonic number.
 * Again, that is normally what you want.
 * You must provide at least one coefficient.
 */
struct akau_fpcm *akau_generate_fpcm_harmonics(
  const struct akau_fpcm *reference,
  const double *coefv,int coefc,
  int normalize
);

/* Same as akau_generate_fpcm_harmonics(), but read the harmonics as text.
 * Input is whitespace-separated coefficients in 0..1000.
 */
struct akau_fpcm *akau_decode_fpcm_harmonics(
  const struct akau_fpcm *reference,
  const char *src,int srcc,
  int normalize
);

/* Return the step in hertz for the given pitch.
 * Pitch zero is A0, ie 27.5 Hz.
 * A4 (440 Hz) is pitch 0x30.
 * A10 (28160 Hz) is pitch 0x78, that should be outside audibility.
 * C21, pitch 0xff, is the highest representable pitch.
 */
double akau_rate_from_pitch(uint8_t pitch);

/* Create a new PCM object by interpolating samples.
 * Note that we do not record the sample rate within the object.
 * This always produces a new object, or fails.
 */
struct akau_ipcm *akau_resample_ipcm(struct akau_ipcm *src,int from_hz,int to_hz);
struct akau_fpcm *akau_resample_fpcm(struct akau_fpcm *src,int from_hz,int to_hz);

/* Convenience to run a wavegen decoder once and capture its output.
 */
struct akau_ipcm *akau_wavegen_decode(const char *src,int srcc);

#define AKAU_WAVEGEN_K_NOOP        0 /* To establish end time, perhaps? */
#define AKAU_WAVEGEN_K_STEP        1 /* v in Hz presumably */
#define AKAU_WAVEGEN_K_TRIM        2 /* v in 0..1 */

struct akau_wavegen_decoder *akau_wavegen_decoder_new();
void akau_wavegen_decoder_del(struct akau_wavegen_decoder *decoder);

/* Build up the program for this decoder.
 * Decoder has a fixed channel list, with one immutable fpcm per channel.
 * You then apply commands, telling us that at a given frame, this channel's step or trim should equal the given value.
 * We fill in the rest, sliding linearly between these explicit control points.
 */
int akau_wavegen_decoder_add_channel(struct akau_wavegen_decoder *decoder,struct akau_fpcm *fpcm);
int akau_wavegen_decoder_add_command(struct akau_wavegen_decoder *decoder,int framep,int chanid,int k,double v);

/* Rather than building up the program manually, you can provide formatted text.
 * This clears the program first.
 */
int akau_wavegen_decoder_decode(struct akau_wavegen_decoder *decoder,const char *src,int srcc);

/* Run the decoder's program and emit an ipcm ready for use.
 */
struct akau_ipcm *akau_wavegen_decoder_print(struct akau_wavegen_decoder *decoder);

int akau_wavegen_decoder_get_length(const struct akau_wavegen_decoder *decoder);

/******************************************************************************
 * WAVEGEN TEXT FORMAT
 *-----------------------------------------------------------------------------
 * Entire file is processed linewise, with '#' beginning a line comment.
 * Each non-empty line declares either a channel or a command.
 * Logically, channel declarations should come first, but we don't require that.
 * 
 * Channel declaration:
 *   channel HARMONICS
 *   channel sine
 *   channel square
 *   channel saw
 *   channel noise
 * Each channel gets an ID sequentially from zero.
 *
 * Command:
 *   TIME CHANID KEY VALUE
 * TIME is a decimal count of milliseconds.
 * CHANID is a decimal integer, first channel declared is ID zero.
 * KEY is one of: step trim noop
 * VALUE is a decimal float, no exponent or negatives.
 *
 *****************************************************************************/

#endif
