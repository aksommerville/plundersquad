#include "test/ps_test.h"
#include "opt/alsa/ps_alsa.h"
#include <unistd.h>
#include <math.h>

/*
AUDIO:INFO: ps_alsa_quit... [src/test/opt/alsa/test_alsa_hang.c:48]
:DEBUG: (Cutting off ALSA output.) [src/os/ps_emergency_abort.c:88]       QUIT
:DEBUG: (Sending audio to ALSA.) [src/os/ps_emergency_abort.c:88]       cb
:DEBUG: (Cancelling audio thread.) [src/os/ps_emergency_abort.c:88]       QUIT
:DEBUG: (Joining audio thread.) [src/os/ps_emergency_abort.c:88]          QUIT
:DEBUG: (Closing ALSA connection.) [src/os/ps_emergency_abort.c:88]       QUIT
test: ../nptl/pthread_mutex_lock.c:359: __pthread_mutex_lock_full: Assertion `robust || (oldval & 0x40000000) == 0' failed.

If I comment out snd_pcm_close(), and just don't close it, we do OK.
Did see one mutex error during init, when not closing.

Trying an extra snd_pcm_drop() before snd_pcm_close()
...and snd_pcm_drop() froze. brilliant.

Added this to the front of the io thread and it seems to work now:
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
*/

/* Trivial audio generator.
 */
 
#define SAMPLEC (44100/400)
 
static int16_t wave[SAMPLEC];
static int wavep=0;
 
static void cb_audio(int16_t *dst,int dstc) {
  for (;dstc-->0;dst++) {
    *dst=wave[wavep];
    if (++wavep>=SAMPLEC) wavep=0;
  }
  //usleep(100000);
}

static int init_audio() {
  int i=SAMPLEC; while (i-->0) {
    wave[i]=sin((i*M_PI*2.0)/SAMPLEC)*10000.0;
  }
  return 0;
}

/* Main test.
 */
 
PS_TEST(test_alsa_hang,functional,ignore) {

  PS_ASSERT_CALL(init_audio())
  
  int bigrepc=20;
  while (bigrepc-->0) {

  ps_log(AUDIO,INFO,"ps_alsa_init...");
  PS_ASSERT_CALL(ps_alsa_init(44100,1,cb_audio))
  ps_log(AUDIO,INFO,"ps_alsa_init ok");
  
  int repc=20+bigrepc*2;
  while (repc-->0) {
    usleep(10000);
  }
  
  ps_log(AUDIO,INFO,"ps_alsa_quit...");
  ps_alsa_quit();
  ps_log(AUDIO,INFO,"ps_alsa_quit ok");
  
  }
  
  return 0;
}
