#include "ps.h"
#include "ps_msaudio.h"
#include <windows.h>
#include <mmsystem.h>

/* Globals.
 */

static struct {
  int init;
  HWAVEOUT waveout;
  void (*cb_audio)(int16_t *dst,int dstc);
  WAVEHDR bufv[2];
  int bufp;
  HANDLE thread;
  HANDLE thread_terminate;
  HANDLE thread_complete;
  HANDLE buffer_ready;
  HANDLE mutex;
} ps_msaudio={0};

/* Callback.
 */

static void CALLBACK ps_msaudio_cb(
  HWAVEOUT hwo,UINT uMsg,DWORD_PTR dwInstance,DWORD dwParam1,DWORD dwParam2
) {
  if (!ps_msaudio.cb_audio) {
    ps_log(MSAUDIO,ERROR,"ps_msaudio_cb(%p,%u,%p,%d,%d)",hwo,uMsg,dwInstance,dwParam1,dwParam2);
    return;
  }
  switch (uMsg) {
    case MM_WOM_OPEN: ps_log(MSAUDIO,DEBUG,"MM_WOM_OPEN"); break;
    case MM_WOM_CLOSE: ps_log(MSAUDIO,DEBUG,"MM_WOM_CLOSE"); break;
    case MM_WOM_DONE: {
        //ps_log(MSAUDIO,DEBUG,"MM_WOM_DONE");
        SetEvent(ps_msaudio.buffer_ready);
      } break;
    default: ps_log(MSAUDIO,DEBUG,"ps_msaudio_cb(%p,%u,%p,%d,%d)",hwo,uMsg,dwInstance,dwParam1,dwParam2);
  }
}

/* Thread main.
 */

static void ps_msaudio_check_buffer(WAVEHDR *hdr) {
  if (hdr->dwUser) return;
  //ps_log(MSAUDIO,TRACE,"fire audio callback");

  if (WaitForSingleObject(ps_msaudio.mutex,INFINITE)) {
    ps_log(MSAUDIO,ERROR,"Audio thread failed to acquire mutex.");
    return;
  }
  
  hdr->dwUser=1;
  ps_msaudio.cb_audio((int16_t*)hdr->lpData,hdr->dwBufferLength>>1);

  ReleaseMutex(ps_msaudio.mutex);
  
  hdr->dwBytesRecorded=hdr->dwBufferLength;
  hdr->dwFlags=WHDR_PREPARED;
  waveOutWrite(ps_msaudio.waveout,hdr,sizeof(WAVEHDR));
}

static DWORD WINAPI ps_msaudio_thread(LPVOID arg) {
  ps_log(MSAUDIO,TRACE,"ps_msaudio_thread");
  while (1) {

    /* Check for termination. */
    if (WaitForSingleObject(ps_msaudio.thread_terminate,0)==WAIT_OBJECT_0) {
      break;
    }

    /* Populate any buffers with zero user data. */
    ps_msaudio_check_buffer(ps_msaudio.bufv+ps_msaudio.bufp);
    ps_msaudio_check_buffer(ps_msaudio.bufv+(ps_msaudio.bufp^1));

    /* Sleep for a little. Swap buffers if signalled. */
    if (WaitForSingleObject(ps_msaudio.buffer_ready,1)==WAIT_OBJECT_0) {
      ps_msaudio.bufv[ps_msaudio.bufp].dwUser=0;
      ps_msaudio.bufp^=1;
    }
    
  }
  ps_log(MSAUDIO,TRACE,"ps_msaudio_thread, terminating.");
  SetEvent(ps_msaudio.thread_complete);
  return 0;
}

/* Initialize.
 */

int ps_msaudio_init(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc)
) {
  ps_log(MSAUDIO,TRACE,"ps_msaudio_init");
  if (!cb) return -1;
  if (ps_msaudio.init) return -1;

  ps_msaudio.cb_audio=cb;

  ps_msaudio.mutex=CreateMutex(0,0,0);
  if (!ps_msaudio.mutex) {
    ps_log(MSAUDIO,ERROR,"CreateMutex() failed");
    return -1;
  }

  WAVEFORMATEX format={
    .wFormatTag=WAVE_FORMAT_PCM,
    .nChannels=chanc,
    .nSamplesPerSec=rate,
    .nAvgBytesPerSec=rate*chanc*2,
    .nBlockAlign=chanc*2, /* frame size in bytes */
    .wBitsPerSample=16,
    .cbSize=0,
  };

  //TODO I have observed freezes here. Can't fathom why.
  MMRESULT result=waveOutOpen(
    &ps_msaudio.waveout,
    WAVE_MAPPER,
    &format,
    (DWORD_PTR)ps_msaudio_cb,
    0,
    CALLBACK_FUNCTION
  );
  ps_log(MSAUDIO,TRACE,"waveOutOpen: %d",result);
  if (result!=MMSYSERR_NOERROR) return -1;

  // 1024 = unintelligible, noisy
  // 8192 = choppy
  // 16384 = fine, with some latency
  // grrr... Now 8192 sounds fine.
  int buffer_size=8192; // bytes (TODO optimal size?)
  WAVEHDR *a=ps_msaudio.bufv+0;
  WAVEHDR *b=ps_msaudio.bufv+1;
  a->dwBufferLength=b->dwBufferLength=buffer_size;
  a->dwBytesRecorded=b->dwBytesRecorded=0;
  a->dwUser=b->dwUser=0;
  a->dwFlags=b->dwFlags=0;
  a->dwLoops=b->dwLoops=0;
  a->lpNext=b->lpNext=0;
  a->reserved=b->reserved=0;
  if (!(a->lpData=calloc(1,buffer_size))) return -1;
  if (!(b->lpData=calloc(1,buffer_size))) return -1;

  if (!(ps_msaudio.thread_terminate=CreateEvent(0,0,0,0))) return -1;
  if (!(ps_msaudio.thread_complete=CreateEvent(0,0,0,0))) return -1;
  if (!(ps_msaudio.buffer_ready=CreateEvent(0,0,0,0))) return -1;

  ps_msaudio.thread=CreateThread(
    0, /* LPSECURITY_ATTRIBUTES */
    0, /* dwStackSize */
    (LPTHREAD_START_ROUTINE)ps_msaudio_thread,
    0, /* lpParameter */
    0, /* dwCreationFlags */
    0 /* lpThreadId */
  );
  if (!ps_msaudio.thread) return -1;
  
  ps_msaudio.init=1;
  ps_log(MSAUDIO,TRACE,"ps_msaudio_init ok");
  return 0;
}

/* Quit.
 */

void ps_msaudio_quit() {
  ps_log(MSAUDIO,TRACE,"ps_msaudio_quit");
  if (ps_msaudio.init) {
    if (ps_msaudio.thread) {
      WaitForSingleObject(ps_msaudio.mutex,INFINITE);
      SetEvent(ps_msaudio.thread_terminate);
      WaitForSingleObject(ps_msaudio.thread_complete,INFINITE);
    }
    if (ps_msaudio.thread_terminate) {
      CloseHandle(ps_msaudio.thread_terminate);
    }
    if (ps_msaudio.thread_complete) {
      CloseHandle(ps_msaudio.thread_complete);
    }
    if (ps_msaudio.buffer_ready) {
      CloseHandle(ps_msaudio.buffer_ready);
    }
    if (ps_msaudio.waveout) {
      waveOutClose(ps_msaudio.waveout);
    }
    if (ps_msaudio.mutex) {
      CloseHandle(ps_msaudio.mutex);
    }
    if (ps_msaudio.bufv[0].lpData) free(ps_msaudio.bufv[0].lpData);
    if (ps_msaudio.bufv[1].lpData) free(ps_msaudio.bufv[1].lpData);
    ps_msaudio.init=0;
  }
}

/* Lock.
 */

static int ps_msaudio_lock() {
  if (WaitForSingleObject(ps_msaudio.mutex,INFINITE)) {
    ps_log(MSAUDIO,ERROR,"Failed to lock audio mutex.");
    return -1;
  }
  return 0;
}

static int ps_msaudio_unlock() {
  ReleaseMutex(ps_msaudio.mutex);
  return 0;
}

/* Extra glue for akau.
 * You can safely remove this section if you are not using akau.
 */

#include "akau/akau_driver.h"

const struct akau_driver akau_driver_msaudio={
  .init=ps_msaudio_init,
  .quit=ps_msaudio_quit,
  .lock=ps_msaudio_lock,
  .unlock=ps_msaudio_unlock,
};
