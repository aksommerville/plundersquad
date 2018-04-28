#include "test/ps_test.h"
#include "akau/akau_songprinter.h"
#include "akau/akau_pcm.h"
#include "akau/akau.h"
#include "res/ps_resmgr.h"
#include "os/ps_clockassist.h"

extern const struct akau_driver akau_driver_akmacaudio;
extern const struct akau_driver akau_driver_alsa;
#define AUDIODRIVER akau_driver_alsa

static void ps_songprinter_test_log(int level,const char *msg,int msgc) {
  ps_log(TEST,INFO,"[%d] %.*s",level,msgc,msg);
}

static void log_duration(const char *label,int64_t us) {
  int sec=us/1000000; us%=1000000;
  int min=sec/60; sec%=60;
  ps_log(TEST,INFO,"%12s: %02d:%02d.%06d",label,min,sec,(int)us);
}

/* Functional test: Print a song synchronously, dump some info about it, and play it back as IPCM.
 */

PS_TEST(test_songprinter_synchronous,ignore,songprinter) {
  int songid=7;

  akau_quit();
  PS_ASSERT_CALL(akau_init(&AUDIODRIVER,ps_songprinter_test_log))

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  struct akau_song *song=ps_res_get(PS_RESTYPE_SONG,songid);
  PS_ASSERT(song)

  struct akau_songprinter *printer=akau_songprinter_new(song);
  PS_ASSERT(printer)

  int64_t time_start=ps_time_now();
  PS_ASSERT_CALL(akau_songprinter_finish(printer))
  int64_t time_end=ps_time_now();
  int64_t time_elapsed=time_end-time_start;

  struct akau_ipcm *ipcm=akau_songprinter_get_ipcm(printer);
  PS_ASSERT(ipcm);

  int samplec=akau_ipcm_get_sample_count(ipcm);
  int64_t output_duration=(samplec*1000000ll)/44100;
  ps_log(TEST,INFO,"Printed song ID %d...",songid);
  ps_log(TEST,INFO,"Elapsed time %d.%06d s",(int)(time_elapsed/1000000),(int)(time_elapsed%1000000));
  log_duration("Elapsed",time_elapsed);
  log_duration("Output",output_duration);
  ps_log(TEST,INFO,"Tempo %d bpm",akau_song_get_tempo(song));
  ps_log(TEST,INFO,"Beat count %d",akau_song_count_beats(song));
  ps_log(TEST,INFO,"Command count %d",akau_song_count_commands(song));
  ps_log(TEST,INFO,"Sample count %d",samplec);

  struct akau_mixer *mainmixer=akau_get_mixer();
  PS_ASSERT(mainmixer)
  PS_ASSERT_CALL(akau_mixer_play_ipcm(mainmixer,ipcm,0x80,0,0,0))
  while (1) {
    ps_time_sleep(1000000);
    if (!akau_mixer_count_channels(mainmixer)) break;
  }

  ps_resmgr_quit();
  akau_quit();
  return 0;
}

/* Functional test: Print a song asynchronously and confirm asynchronicity by animating something on the console.
 */

PS_TEST(test_songprinter_asynchronous,ignore,songprinter) {
  int songid=7;

  akau_quit();
  PS_ASSERT_CALL(akau_init(&AUDIODRIVER,ps_songprinter_test_log))

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  struct akau_song *song=ps_res_get(PS_RESTYPE_SONG,songid);
  PS_ASSERT(song)

  struct akau_songprinter *printer=akau_songprinter_new(song);
  PS_ASSERT(printer)

  ps_log(TEST,INFO,"Beginning song print...");
  PS_ASSERT_CALL(akau_songprinter_begin(printer))
  int animframe=0;
  printf("|\n");
  while (1) {
    ps_time_sleep(1000);

    int progress=akau_songprinter_get_progress(printer);
    if ((progress<0)||(progress>=100)) {
      printf("progress = %d\n",progress);
      if (progress<0) return -1;
      break;
    }
    
    if (++animframe>=4) animframe=0;
    switch (animframe) {
      case 0: printf("\x1b[A| %d%%\n",progress); break;
      case 1: printf("\x1b[A/ %d%%\n",progress); break;
      case 2: printf("\x1b[A- %d%%\n",progress); break;
      case 3: printf("\x1b[A\\ %d%%\n",progress); break;
    }
  }

  PS_ASSERT_CALL(akau_songprinter_finish(printer))

  struct akau_ipcm *ipcm=akau_songprinter_get_ipcm(printer);
  PS_ASSERT(ipcm);

  int samplec=akau_ipcm_get_sample_count(ipcm);
  int64_t output_duration=(samplec*1000000ll)/44100;
  ps_log(TEST,INFO,"Printed song ID %d...",songid);
  log_duration("Output",output_duration);
  ps_log(TEST,INFO,"Tempo %d bpm",akau_song_get_tempo(song));
  ps_log(TEST,INFO,"Beat count %d",akau_song_count_beats(song));
  ps_log(TEST,INFO,"Command count %d",akau_song_count_commands(song));
  ps_log(TEST,INFO,"Sample count %d",samplec);

  struct akau_mixer *mainmixer=akau_get_mixer();
  PS_ASSERT(mainmixer)
  PS_ASSERT_CALL(akau_mixer_play_ipcm(mainmixer,ipcm,0x80,0,0,0))
  while (1) {
    ps_time_sleep(1000000);
    if (!akau_mixer_count_channels(mainmixer)) break;
  }

  ps_resmgr_quit();
  akau_quit();
  return 0;
}
