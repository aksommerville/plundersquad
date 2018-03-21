/* ps_msaudio.h
 * This has nothing to do with GDI. I'm just dumping it here with all the Windows stuff.
 * Simple callback-oriented audio with the DIB interface.
 * This interface was deprecated as of Windows Vista in favour of DirectAudio and CoreAudio.
 * Unfortunately, stock MinGW doesn't appear to support either of those.
 * DIB should be supported all the way back to Windows 95.
 */

#ifndef PS_MSAUDIO_H
#define PS_MSAUDIO_H

/* Callback receives the output buffer and count of samples expected.
 * Samples, not frames or bytes.
 */
int ps_msaudio_init(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc)
);

void ps_msaudio_quit();

#endif
