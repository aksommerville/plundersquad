#include "test/ps_test.h"
#include "opt/msaudio/ps_msaudio.h"
#include "os/ps_clockassist.h"

/* Dummy callback.
 */

static void audiocb(int16_t *dst,int dstc) {
  memset(dst,0,dstc<<1);
}

/* Bring msaudio online repeatedly to try to duplicate a mysterious freeze I've observed in the field.
 * I believe the culprit was waveOutOpen().
 * Inserted log statements between almost every API call.
 * *** First run:
 *   1000 repetitions in well under a minute, no freeze.
 *   Change to run indefinitely.
 */

/*
TEST:INFO: Repetition 1252, elapsed 21615570... [src/test/opt/msaudio/test_msaudio_functional.c:27]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:97]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:99]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:104]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:122]
MSAUDIO:DEBUG: MM_WOM_OPEN [src/opt/msaudio/ps_msaudio.c:33]
MSAUDIO:TRACE: waveOutOpen: 0 [src/opt/msaudio/ps_msaudio.c:131]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:148]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:152]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:157]
MSAUDIO:TRACE: ps_msaudio_init ok [src/opt/msaudio/ps_msaudio.c:169]
MSAUDIO:TRACE: ps_msaudio_thread [src/opt/msaudio/ps_msaudio.c:66]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:177]
### freeze here ###
Reviewing the log, it is rare to see ps_msaudio_thread before ps_msaudio_quit.
Add a delay on the main thread to see if that forces it.
*/

/*
Now it's "init ok", "thread", "quit", every time. Not freezing.
Added some logging to the thread, and let's try again without the main-thread delay.
TEST:INFO: Repetition 1428, elapsed 153710865... [src/test/opt/msaudio/test_msaudio_functional.c:46]
*/

/*
With the extra logging in background thread, it's running *radically* slower.
*/

/*
Removed delay and background-thread logging, so we're back in the fallible state.
I suspect that we are failing because the background thread calls WaitForSingleObject(mutex) twice between checks of thread_terminate.
grrr.... It doesn't want to happen again.
(TEST:INFO: Repetition 14666, elapsed 253876830... [src/test/opt/msaudio/test_msaudio_functional.c:61])
I'll try with a delay between the two mutex checks, and see if that fails more.
*/

/*
Whoo!
With a delay between the two mutex checks, and another in the main thread before quit, it fails fast:
TEST:INFO: Repetition 6, elapsed 163707... [src/test/opt/msaudio/test_msaudio_functional.c:67]
TEST:INFO: Repetition 18, elapsed 372505... [src/test/opt/msaudio/test_msaudio_functional.c:70]
TEST:INFO: Repetition 5, elapsed 124539... [src/test/opt/msaudio/test_msaudio_functional.c:70]
Now we can start fixing it.
*/

/*
Added a check of thread_terminate in ps_msaudio_check_buffer().
But now it fails even faster:
TEST:INFO: Repetition 1, elapsed 0... [src/test/opt/msaudio/test_msaudio_functional.c:73]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:111]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:113]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:118]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:136]
MSAUDIO:DEBUG: MM_WOM_OPEN [src/opt/msaudio/ps_msaudio.c:33]
MSAUDIO:TRACE: waveOutOpen: 0 [src/opt/msaudio/ps_msaudio.c:145]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:162]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:166]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:171]
MSAUDIO:TRACE: ps_msaudio_init ok [src/opt/msaudio/ps_msaudio.c:183]
MSAUDIO:TRACE: ps_msaudio_thread [src/opt/msaudio/ps_msaudio.c:74]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:191]
...duuurp
That's because I also removed the thread_terminate check in the main background loop.
So, if no buffer was ready to populate, we would never check thread_terminate.
OK, once more...
*/

/*
Still failing:
TEST:INFO: Repetition 5, elapsed 132539... [src/test/opt/msaudio/test_msaudio_functional.c:95]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:109]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:111]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:116]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:134]
MSAUDIO:DEBUG: MM_WOM_OPEN [src/opt/msaudio/ps_msaudio.c:33]
MSAUDIO:TRACE: waveOutOpen: 0 [src/opt/msaudio/ps_msaudio.c:143]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:160]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:164]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:169]
MSAUDIO:TRACE: ps_msaudio_init ok [src/opt/msaudio/ps_msaudio.c:181]
MSAUDIO:TRACE: ps_msaudio_thread [src/opt/msaudio/ps_msaudio.c:74]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:189]
All repetitions this time were ok-thread-quit
*/

/*
Added more logging in ps_msaudio_quit()...
TEST:INFO: Repetition 9, elapsed 206531... [src/test/opt/msaudio/test_msaudio_functional.c:117]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:109]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:111]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:116]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:134]
MSAUDIO:DEBUG: MM_WOM_OPEN [src/opt/msaudio/ps_msaudio.c:33]
MSAUDIO:TRACE: waveOutOpen: 0 [src/opt/msaudio/ps_msaudio.c:143]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:160]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:164]
MSAUDIO:TRACE: ps_msaudio_init [src/opt/msaudio/ps_msaudio.c:169]
MSAUDIO:TRACE: ps_msaudio_init ok [src/opt/msaudio/ps_msaudio.c:181]
MSAUDIO:TRACE: ps_msaudio_thread [src/opt/msaudio/ps_msaudio.c:74]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:189]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:192]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:194]
MSAUDIO:TRACE: ps_msaudio_quit [src/opt/msaudio/ps_msaudio.c:196]
It's waiting for thread_complete
*/

/*
Changed the ps_msaudio_check_buffer mutex lock from INFINITE to 500, and now it seems satisfied.
I see some of these in the log too:
MSAUDIO:ERROR: Audio thread failed to acquire mutex. [src/opt/msaudio/ps_msaudio.c:52]
*/

/*
Dropped that timeout to 10.
Seems to run fine.
TEST:INFO: Repetition 2823, elapsed 1153,557773... [src/test/opt/msaudio/test_msaudio_functional.c:140]
19 minutes without a failure, that seems good.
*/

/*
Except that now, waveOutOpen() is stalling every time.
Not actually freezing, but it can take more than a second to return.
And running the real game now, audio output is noisy as if it's consistently underrunning.
What the fuck???
*/

/*
Rebooted and now it's running fine again.
All these starts and disorderly stops must have broken the driver somehow.
Fucking Windows.
*/

/*
I'm about to run into the office for a bit. Leave this test running open-ended for a few hours while I'm gone...
Starts out zippity-quick, but by 1200 or 1300 it has slowed to like half a second per iteration.
Thought to check Performance Monitor after it was well underway.
"Committed Memory" was running around 7.4 GB.
"% Committed Bytes In Use" was in the 70s and climbing.
Check these after a reboot.
Terminating the test did not immediately change much, but % Committed starting falling instead of climbing.
*/

/*
Running from a fresh boot with perfmon and taskmgr running.
Committed Bytes before start is under 2 GB.
% Committed Bytes In use hovering around 20%.
Sure enough, both indicators climbed steadily, to about 60% of 5 GB when I aborted (1775 repetitions).
After aborting, both indicators dropped slowly, then accelerating, and a few minutes later were right where we started.
CPU time also dropped from around 10% to around 1% right at the last drop-off.
So.... is there some driver process that remains loaded for several minutes after waveOutClose()?
*/

/*
If I watch taskmgr it's obvious: Windows Audio Device Graph Isolation, audiodg.exe
It quickly balloons to multiple gigabytes of RAM.
"Realtek HD Audio Manager" is a bloatware control panel that came with my motherboard.
In there, audio output was set to 24 bit @ 48 kHz.
Changed to 16 bit @ 44.1 kHz, retrying...
...nope, same misbehavior.
*/

/*
What does audiodg look like when just playing the game?
...indicators look fine, but I'm not hearing any audio either...?
Nevermind, I had the wrong jack selected. It's fine.
*/

/*
What I'm reading online hints that audiodg is a wrapper around drivers provided by OEMs.
So, this might be a problem with my motherboard but not others.
The production code works, only my test is fucked. So whatever.
*/

/*
I hate to leave it there.
Trying with just one repetition, and running the process multiple times.
Thinking that maybe the parent process matters to the driver...?
*/

PS_TEST(test_msaudio_startup_freeze,ignore) {
  ps_msaudio_quit();

  int64_t starttime=ps_time_now();
  int repc=0;
  while (1) {
    repc++;
    ps_log(TEST,INFO,"Repetition %d, elapsed %lld...",repc,ps_time_now()-starttime);
    PS_ASSERT_CALL(ps_msaudio_init(44100,2,audiocb))
    ps_time_sleep(1000);
    break;
    ps_msaudio_quit();
  }

  ps_msaudio_quit();
  return 0;
}
