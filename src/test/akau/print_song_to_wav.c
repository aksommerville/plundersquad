#include "test/ps_test.h"
#include "akau/akau_songprinter.h"
#include "akau/akau_pcm.h"
#include "akau/akau.h"
#include "res/ps_resmgr.h"
#include "os/ps_clockassist.h"
#include <unistd.h>
#include <fcntl.h>

extern const struct akau_driver akau_driver_akmacaudio;
extern const struct akau_driver akau_driver_alsa;
extern const struct akau_driver akau_driver_msaudio;
#if PS_USE_akmacaudio
#define AUDIODRIVER akau_driver_akmacaudio
#elif PS_USE_alsa
#define AUDIODRIVER akau_driver_alsa
#elif PS_USE_msaudio
#define AUDIODRIVER akau_driver_msaudio
#endif

static void ps_songprinter_test_log(int level,const char *msg,int msgc) {
  ps_log(TEST,INFO,"[%d] %.*s",level,msgc,msg);
}

static void log_duration(const char *label,int64_t us) {
  int sec=us/1000000; us%=1000000;
  int min=sec/60; sec%=60;
  ps_log(TEST,INFO,"%12s: %02d:%02d.%06d",label,min,sec,(int)us);
}

/* Compose path for output.
 */
 
static int compose_output_path_for_printed_song(char *dst,int dsta,int songid) {
  char srcpath[1024];
  int srcpathc=ps_res_get_path_for_resource(srcpath,sizeof(srcpath),PS_RESTYPE_SONG,songid,0);
  if ((srcpathc<1)||(srcpathc>sizeof(srcpath))) {
    int dstc=snprintf(dst,dsta,"song-%03d.wav",songid);
    if ((dstc<1)||(dstc>=dsta)) return -1;
    return dstc;
  }
  int slashp=srcpathc;
  while ((slashp-->0)&&(srcpath[slashp]!='/')) ;
  const char *srcbase=srcpath+slashp+1;
  int srcbasec=srcpathc-slashp-1;
  if ((srcbasec>0)&&(srcbasec<dsta-4)) {
    memcpy(dst,srcbase,srcbasec);
    memcpy(dst+srcbasec,".wav",4);
    int dstc=srcbasec+4;
    dst[dstc]=0;
    return dstc;
  }
  int dstc=snprintf(dst,dsta,"%d.wav",songid);
  if ((dstc<1)||(dsta>=dsta)) return -1;
  return dstc;
}

/* Write a WAV file, given raw samples.
 * (srcc) is in bytes.
 */
 
static void wr32le(uint8_t *dst,int src) {
  dst[0]=src;
  dst[1]=src>>8;
  dst[2]=src>>16;
  dst[3]=src>>24;
}

static void wr16le(uint8_t *dst,int src) {
  dst[0]=src;
  dst[1]=src>>8;
}
 
static int write_wav_file(const char *path,const void *src,int srcc) {
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) return -1;
  
  /* Header format described at: http://soundfile.sapp.org/doc/WaveFormat/
   */
  uint8_t hdr[44]={0};
  memcpy(hdr,"RIFF",4);
  wr32le(hdr+4,36+srcc); // 36 == sizeof(hdr) minus signature and this size field
  memcpy(hdr+8,"WAVE",4);
  memcpy(hdr+12,"fmt ",4);
  wr32le(hdr+16,16);
  wr16le(hdr+20,1); // AudioFormat (PCM)
  wr16le(hdr+22,1); // NumChannels
  wr32le(hdr+24,44100); // SampleRate
  wr32le(hdr+28,88200); // ByteRate
  wr16le(hdr+32,2); // BlockAlign
  wr16le(hdr+34,16); // BitsPerSample
  memcpy(hdr+36,"data",4);
  wr32le(hdr+40,srcc);
  
  int hdrp=0;
  while (hdrp<sizeof(hdr)) {
    int err=write(fd,hdr+hdrp,sizeof(hdr)-hdrp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    hdrp+=err;
  }
  
  /* Follow the header with raw data; it's already formatted correctly.
   */
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  
  close(fd);
  return 0;
}

/* Print a song synchronously, then commit it to disk as a WAV file.
 * It's implemented as a "test" for convenience.
 */

PS_TEST(print_song_to_wav,ignore) {
  int songid=1;

  akau_quit();
  PS_ASSERT_CALL(akau_init(&AUDIODRIVER,ps_songprinter_test_log))

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))
  
  char dstpath[1024];
  int dstpathc=compose_output_path_for_printed_song(dstpath,sizeof(dstpath),songid);
  PS_ASSERT_INTS_OP(dstpathc,>,0)
  PS_ASSERT_INTS_OP(dstpathc,<,sizeof(dstpath))

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

  void *samplev=akau_ipcm_get_sample_buffer(ipcm);
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
  ps_log(TEST,INFO,"Print rate: %.06f",(double)time_elapsed/(double)output_duration);
  
  PS_ASSERT_CALL(write_wav_file(dstpath,samplev,samplec<<1))

  ps_resmgr_quit();
  akau_quit();
  return 0;
}
