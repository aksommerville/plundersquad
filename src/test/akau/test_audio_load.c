#include "test/ps_test.h"
#include "os/ps_clockassist.h"
#include "akau/akau.h"

/* Driver selection.
 */
 
#if PS_USE_alsa
  #include "opt/alsa/ps_alsa.h"
  #define driver &akau_driver_alsa
#endif

/* Callback.
 */
 
static void cb_akau_log(int level,const char *msg,int msgc) {
  ps_log(TEST,INFO,"akau:%d: %.*s",level,msgc,msg);
}

/* Generate some sound effects and load them into AKAU.
 */
 
static int load_test_sounds() {
  // Fuck it, let's just use the real data:
  PS_ASSERT_CALL(akau_load_resources("src/data"))
  return 0;
}

/* Pick an IPCM resource at random and play it.
 */
 
static int play_random_ipcm() {
  const struct akau_store *store=akau_get_store();
  int ipcmc=akau_store_count_ipcm(store);
  PS_ASSERT_INTS_OP(ipcmc,>,0)
  int resix=rand()%ipcmc;
  int resid=akau_store_get_ipcm_id_by_index(store,resix);
  PS_ASSERT_INTS_OP(resid,>=,0,"ix %d/%d",resix,ipcmc)
  uint8_t trim=0x40+(rand()%0xbf);
  int8_t pan=(rand()&0xff)-0x80;
  PS_ASSERT_CALL(akau_play_sound(resid,trim,pan))
  return 0;
}

/* Audio load test, main entry point.
 * It sounds like the spaceship is entering warp!
 * Even better: it segfaults eventually! 
 * Takes a few seconds, but I've seen segfaults now 3 times in a row. (repc=10000, sleep=10000)
 * Sleeping 10000 us causes about 40 channels to remain in play consistently.
 * ...inside AKAU's callback.
 * Added lock to akau_play_sound_as() (+loop, +song)... ran 6 times in a row without failing.
 */
 
PS_TEST(test_audio_load,ignore,functional) {

  int randseed=ps_time_now();
  ps_log(TEST,INFO,"randseed %d",randseed);
  srand(randseed);

  akau_quit();
  PS_ASSERT_CALL(akau_init(driver,cb_akau_log))
  PS_ASSERT_CALL(load_test_sounds())
  
  int repc=10000;
  while (repc-->0) {
    ps_time_sleep(10000);
    PS_ASSERT_CALL(play_random_ipcm())
    PS_ASSERT_CALL(akau_update())
  }
  
  akau_quit();
  return 0;
}
