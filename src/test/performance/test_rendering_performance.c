/* test_rendering_performance.c
 *
 * We setup mostly like the real game.
 * No audio or GUI, but pretty much everything else is just like production.
 * Then we compose a few scenes and render them thousands of times to measure performance.
 * In particular, I want to see if it is worth optimizing the software renderer further.
 *
 * With the test running, press B to create an explosion (lots of bells-and-whistles sprites).
 * It will draw each frame repeatedly and record the processor time spent on each cycle.
 * Our timing only records time spent in this process, not calendar time.
 * And it doesn't record the game's update, the final video delivery, or any other processing.
 * Each log entry is: draw count, clock count, average aprite count, average time per draw.
 *
 * TEST RESULTS: Windows with soft render on Dell Latitude E5400 (Core 2 Duo @ 2 GHz, Windows 7)
 * This machine doesn't support OpenGL 2, and is the reason I wrote the software renderer to begin with.
MAIN:INFO:        500        749          1 0.001498000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        718          1 0.001436000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        702          1 0.001404000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        717          1 0.001434000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        719          1 0.001438000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        718          1 0.001436000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        718          1 0.001436000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        843          5 0.001686000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       1358         22 0.002716000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       1669         30 0.003338000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       2387         51 0.004774000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3355         80 0.006710000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3369         85 0.006738000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3416         85 0.006832000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3417         85 0.006834000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3308         85 0.006616000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       3104         85 0.006208000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       2823         85 0.005646000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       2652         85 0.005304000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       2372         77 0.004744000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       2106         65 0.004212000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       1732         53 0.003464000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500       1280         33 0.002560000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        702          5 0.001404000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        702          5 0.001404000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        717          5 0.001434000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        717          5 0.001434000 [src/test/performance/test_rendering_performance.c:110]
MAIN:INFO:        500        703          5 0.001406000 [src/test/performance/test_rendering_performance.c:110]
 *
 * TEST RESULTS: MacOS with hardware render.
 * I can't launch this test on the Mac due to IoC (grrrrr), but modified ps_main to do about the same thing.
MAIN:INFO:        500      85244          1 0.000170488 [src/main/ps_main.c:64]
MAIN:INFO:        500      84431          1 0.000168862 [src/main/ps_main.c:64]
MAIN:INFO:        500      80396          1 0.000160792 [src/main/ps_main.c:64]
MAIN:INFO:        500      81281          1 0.000162562 [src/main/ps_main.c:64]
MAIN:INFO:        500      85669          1 0.000171338 [src/main/ps_main.c:64]
MAIN:INFO:        500     111109         22 0.000222218 [src/main/ps_main.c:64]
MAIN:INFO:        500      98911         22 0.000197822 [src/main/ps_main.c:64]
MAIN:INFO:        500     142596        662 0.000285192 [src/main/ps_main.c:64]
MAIN:INFO:        500     145764        665 0.000291528 [src/main/ps_main.c:64]
MAIN:INFO:        500     157340        634 0.000314680 [src/main/ps_main.c:64]
MAIN:INFO:        500     144554        590 0.000289108 [src/main/ps_main.c:64]
MAIN:INFO:        500     142166        542 0.000284332 [src/main/ps_main.c:64]
 *
 * TEST RESULTS: MacOS with software render.
MAIN:INFO:        500     337076          1 0.000674152 [src/main/ps_main.c:64]
MAIN:INFO:        500     576540         30 0.001153080 [src/main/ps_main.c:64]
MAIN:INFO:        500     772812         43 0.001545624 [src/main/ps_main.c:64]
MAIN:INFO:        500     794661         43 0.001589322 [src/main/ps_main.c:64]
MAIN:INFO:        500     686465         43 0.001372930 [src/main/ps_main.c:64]
MAIN:INFO:        500     653660         43 0.001307320 [src/main/ps_main.c:64]
MAIN:INFO:        500     662207         43 0.001324414 [src/main/ps_main.c:64]
MAIN:INFO:        500     652886         43 0.001305772 [src/main/ps_main.c:64]
MAIN:INFO:        500     622620         43 0.001245240 [src/main/ps_main.c:64]
MAIN:INFO:        500     630738         47 0.001261476 [src/main/ps_main.c:64]
MAIN:INFO:        500     883066         89 0.001766132 [src/main/ps_main.c:64]
MAIN:INFO:        500    1286438        143 0.002572876 [src/main/ps_main.c:64]
MAIN:INFO:        500    1589038        190 0.003178076 [src/main/ps_main.c:64]
MAIN:INFO:        500    1771995        200 0.003543990 [src/main/ps_main.c:64]
MAIN:INFO:        500    1759127        192 0.003518254 [src/main/ps_main.c:64]
MAIN:INFO:        500    1716658        192 0.003433316 [src/main/ps_main.c:64]
MAIN:INFO:        500    1763854        192 0.003527708 [src/main/ps_main.c:64]
MAIN:INFO:        500    1695826        192 0.003391652 [src/main/ps_main.c:64]
MAIN:INFO:        500    1620055        192 0.003240110 [src/main/ps_main.c:64]
MAIN:INFO:        500    1456342        192 0.002912684 [src/main/ps_main.c:64]
MAIN:INFO:        500    1385796        192 0.002771592 [src/main/ps_main.c:64]
MAIN:INFO:        500    1247136        184 0.002494272 [src/main/ps_main.c:64]
MAIN:INFO:        500     994086        136 0.001988172 [src/main/ps_main.c:64]
MAIN:INFO:        500     713026         88 0.001426052 [src/main/ps_main.c:64]
MAIN:INFO:        500     441901         36 0.000883802 [src/main/ps_main.c:64]
MAIN:INFO:        500     340196         12 0.000680392 [src/main/ps_main.c:64]
 *
 * TEST RESULTS: Linux with hardware render on Asus (Core 2 Duo 2.13 GHz, Debian 3.16.51-3)
 * With these test parameters, I couldn't even perceive any slowdown. :)
MAIN:INFO:        500      59508        304 0.000119016 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      39438        309 0.000078876 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      44162        313 0.000088324 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      57202        322 0.000114404 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      38499        323 0.000076998 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      47784        328 0.000095568 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      41733        334 0.000083466 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      56622        339 0.000113244 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      59246        344 0.000118492 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      44669        345 0.000089338 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      43843        346 0.000087686 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      53094        346 0.000106188 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      39970        326 0.000079940 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      56501        306 0.000113002 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      41014        282 0.000082028 [src/test/performance/test_rendering_performance.c:120]
MAIN:INFO:        500      44646        258 0.000089292 [src/test/performance/test_rendering_performance.c:120]
 *
 * TEST RESULTS: Linux with software render on Asus
MAIN:INFO:        500    1116389          1 0.002232778 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1116958          1 0.002233916 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1114766          1 0.002229532 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1114022          1 0.002228044 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1115282          1 0.002230564 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1114300          1 0.002228600 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1117234          1 0.002234468 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1115201          1 0.002230402 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1221886          9 0.002443772 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1389835         22 0.002779670 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1440114         26 0.002880228 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1660534         43 0.003321068 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1944684         64 0.003889368 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1938079         64 0.003876158 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2269575         89 0.004539150 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2470823        106 0.004941646 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2456133        106 0.004912266 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2432986        106 0.004865972 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2386071        106 0.004772142 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2311121        106 0.004622242 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    2149370         94 0.004298740 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1999921         86 0.003999842 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1867163         78 0.003734326 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1692217         62 0.003384434 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1517463         46 0.003034926 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1465264         46 0.002930528 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1200295         14 0.002400590 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1131255          6 0.002262510 [src/test/performance/test_rendering_performance.c:137]
MAIN:INFO:        500    1133499          6 0.002266998 [src/test/performance/test_rendering_performance.c:137]
 *
 * TEST RESULTS: Raspberry Pi with hardware render.
MAIN:INFO:        500     535050          1 0.001070100 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     546770          1 0.001093540 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     542457          1 0.001084914 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     543358          1 0.001086716 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     545761          1 0.001091522 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     545218          1 0.001090436 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     545444          1 0.001090888 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     574080          5 0.001148160 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     694404         34 0.001388808 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     702270         43 0.001404540 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     711945         51 0.001423890 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     728909         68 0.001457818 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     746572         85 0.001493144 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     748865         93 0.001497730 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     771305        127 0.001542610 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     790759        148 0.001581518 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     787941        148 0.001575882 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     790264        148 0.001580528 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     791949        148 0.001583898 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     780585        140 0.001561170 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     760847        112 0.001521694 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     758247        108 0.001516494 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     751778         96 0.001503556 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     735589         80 0.001471178 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     726238         68 0.001452476 [src/test/performance/test_rendering_performance.c:202]
MAIN:INFO:        500     716361         56 0.001432722 [src/test/performance/test_rendering_performance.c:202]
 *
 * TEST RESULTS: Raspberry Pi with software render
 * This doesn't actually work, because with the SOFT strategy, the copy to screen uses GL 1.
 * I'm not going to fix that just for a performance test, but the actual part we're measuring is still valid.
 * ...Even if we fixed it, this is what we'd be dealing with:
MAIN:INFO:        500    4498568          1 0.008997136 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500    4559213          1 0.009118426 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500    4559125          1 0.009118250 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500    5925833         17 0.011851666 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500    6942708         29 0.013885416 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500    9236973         57 0.018473946 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   10945434         77 0.021890868 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   11336821         81 0.022673642 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   11271718         81 0.022543436 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   11071621         81 0.022143242 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   10802269         81 0.021604538 [src/test/performance/test_rendering_performance.c:228]
MAIN:INFO:        500   10526867         81 0.021053734 [src/test/performance/test_rendering_performance.c:228]
 *
 */

#include "test/ps_test.h"
#include "os/ps_ioc.h"
#include "os/ps_fs.h"
#include "os/ps_userconfig.h"
#include "video/ps_video.h"
#include "input/ps_input.h"
#include "input/ps_input_button.h"
#include "res/ps_resmgr.h"
#include "game/ps_game.h"
#include "akgl/akgl.h"
#include <time.h>

#if PS_USE_macioc
  #include "opt/macioc/ps_macioc.h"
#endif
#if PS_USE_macwm
  #include "opt/macwm/ps_macwm.h"
#endif
#if PS_USE_machid
  #include "opt/machid/ps_machid.h"
#endif
#if PS_USE_evdev
  #include "opt/evdev/ps_evdev.h"
#endif
#if PS_USE_glx
  #include "opt/glx/ps_glx.h"
#endif
#if PS_USE_mswm
  #include "opt/mswm/ps_mswm.h"
#endif
#if PS_USE_mshid
  #include "opt/mshid/ps_mshid.h"
#endif

/* Globals.
 */

static struct ps_game *ps_game=0;

/* Test results.
 */

#define REPORT_INTERVAL 5 /* Gather stats for so many frame, then report. */
#define DRAWS_PER_FRAME 100

static int64_t drawc=0;
static clock_t total_elapsed=0;
static int total_sprites=0;
static int iterationp=0;

static void record_test_result(int repc,clock_t elapsed) {
  drawc+=repc;
  total_elapsed+=elapsed;
  total_sprites+=ps_game->grpv[PS_SPRGRP_KEEPALIVE].sprc;
  if (++iterationp>=REPORT_INTERVAL) {
    double average=((double)total_elapsed)/((double)drawc*CLOCKS_PER_SEC);
    int spritec=total_sprites/iterationp;
    ps_log(MAIN,INFO,"%10lld %10lld %10d %.09f",(long long)drawc,(long long)total_elapsed,spritec,average);
    iterationp=0;
    drawc=0;
    total_elapsed=0;
    total_sprites=0;
  }
}

/* Setup for quick testing during development.
 * This is called whenever no saved game was requested.
 * Return 0 to fall back to interactive setup -- production builds should always do that.
 */

static int ps_setup_test_game(struct ps_userconfig *userconfig) {
  int i;

  /* Configure players. */
  if (ps_game_set_player_count(ps_game,1)<0) return -1;
  for (i=1;i<=PS_PLAYER_LIMIT;i++) {
    ps_game_configure_player(ps_game,i,i,i,0);
  }
  if (ps_input_set_noninteractive_device_assignment()<0) return -1;

  /* Optionally override plrdef selection. (plrid,plrdefid,palette,device) */
  //001-swordsman 002-archer    003-gadgeteer 004-nurse     005-wizard    006-vampire   007-martyr    008-immortal  009-bomber
  if (ps_game_configure_player(ps_game,1,1,0,0)<0) return -1;

  if (ps_game_generate_test(ps_game,
    -1, // regionid, negative means random
    // blueprintid. At least one must have adequate HERO POI:
    2,1
  )<0) return -1;

  if (ps_game_restart(ps_game)<0) return -1;

  return 1;
}

/* Init input.
 */

static int ps_main_init_input(struct ps_userconfig *userconfig) {

  /* Initialize generic input core. */
  if (ps_input_init()<0) return -1;

  /* Initialize and connect providers. */
  #if PS_USE_macioc
    if (ps_macioc_connect_input()<0) return -1;
  #endif
  #if PS_USE_macwm
    if (ps_macwm_connect_input()<0) return -1;
  #endif
  #if PS_USE_machid
    {
      if (ps_machid_init(&ps_machid_default_delegate)<0) return -1;
      struct ps_input_provider *provider=ps_machid_new_provider();
      if (ps_input_install_provider(provider)<0) return -1;
    }
  #endif
  #if PS_USE_evdev
    if (ps_evdev_init_default()<0) return -1;
  #endif
  #if PS_USE_glx
    if (ps_glx_connect_input()<0) return -1;
  #endif
  #if PS_USE_mswm
    if (ps_mswm_connect_input()<0) return -1;
  #endif
  #if PS_USE_mshid
    if (ps_mshid_init()<0) return -1;
  #endif

  /* Load configuration and take it live. */
  const char *input_config_path=0;
  if (ps_userconfig_peek_field_as_string(&input_config_path,userconfig,ps_userconfig_search_field(userconfig,"input",5))<0) return -1;
  if (ps_input_load_configuration(input_config_path)<0) return -1;
  if (ps_input_update()<0) return -1; // Reassigns input devices and gets the core running.
  
  return 0;
}

/* Init.
 */

static int ps_main_init(struct ps_userconfig *userconfig) {
  ps_log(MAIN,TRACE,"%s",__func__);

  if (ps_userconfig_set(userconfig,"soft-render",11,"1",1)<0) return -1;

  int randseed=time(0);
  ps_log(MAIN,INFO,"Random seed %d.",randseed);
  srand(randseed);

  if (ps_video_init(userconfig)<0) return -1;

  if (ps_main_init_input(userconfig)<0) {
    ps_log(MAIN,ERROR,"Failed to initialize input.");
    return -1;
  }

  const char *resources_path=0;
  if (ps_userconfig_peek_field_as_string(&resources_path,userconfig,ps_userconfig_search_field(userconfig,"resources",9))<0) return -1;
  if (ps_resmgr_init(resources_path,0)<0) return -1;

  if (!(ps_game=ps_game_new())) return -1;

  if (ps_setup_test_game(userconfig)<0) return -1;
  
  return 0;
}

/* Quit.
 */

static void ps_main_quit() {
  ps_log(MAIN,TRACE,"%s",__func__);
  
  ps_game_del(ps_game);
  
  ps_resmgr_quit();
  
  ps_input_quit();
  
  ps_video_quit();
  
  #if PS_USE_machid
    ps_machid_destroy_global_provider();
    ps_machid_quit();
  #endif

  #if PS_USE_evdev
    ps_evdev_quit();
  #endif

  #if PS_USE_mshid
    ps_mshid_quit();
  #endif
}

/* Update.
 */

static int button_pressed=0;

static int ps_main_update() {
  //ps_log(MAIN,TRACE,"%s",__func__);

  if (ps_input_update()<0) return -1;

  if (ps_input_termination_requested()) {
    ps_ioc_quit(0);
  }

  /* Create an explosion (ie lots of bells-and-whistles sprites) when B button pressed. */
  if (ps_get_player_buttons(0)&PS_PLRBTN_B) {
    if (!button_pressed) {
      button_pressed=1;
      if (ps_game_decorate_monster_death(ps_game,PS_SCREENW>>1,PS_SCREENH>>1)<0) return -1;
    }
  } else {
    button_pressed=0;
  }

  if (ps_game_update(ps_game)<0) return -1;

  /* Here is the bulk of the test: */
  int repc=DRAWS_PER_FRAME;
  clock_t starttime=clock();
  if (ps_video_test_draw(repc)<0) return -1;
  clock_t endtime=clock();
  record_test_result(repc,endtime-starttime);

  if (ps_video_update()<0) {
    ps_log(MAIN,ERROR,"Error rendering.");
    return -1;
  }

  return 0;
}

/* Main entry point.
 */

PS_TEST(test_rendering_performance,ignore) {
  struct ps_ioc_delegate delegate={
    .init=ps_main_init,
    .quit=ps_main_quit,
    .update=ps_main_update,
  };
  int err=ps_ioc_main(0,0,&delegate);
  if (err) return -1;
  return 0;
}
