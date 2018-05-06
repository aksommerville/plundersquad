#include "ps.h"
#include "ps_scgen.h"
#include "ps_scenario.h"
#include "ps_screen.h"
#include "ps_blueprint.h"
#include "ps_blueprint_list.h"
#include "ps_zone.h"
#include "ps_grid.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "os/ps_clockassist.h"

int ps_scgen_generate_grids(struct ps_scgen *scgen);

/* New.
 */

struct ps_scgen *ps_scgen_new() {

  struct ps_scgen *scgen=calloc(1,sizeof(struct ps_scgen));
  if (!scgen) return 0;

  if (!(scgen->blueprints_scratch=ps_blueprint_list_new())) {
    ps_scgen_del(scgen);
    return 0;
  }

  if (!(scgen->zones=ps_zones_new())) {
    ps_scgen_del(scgen);
    return 0;
  }

  scgen->regionid=-1;

  return scgen;
}

/* Delete.
 */

void ps_scgen_del(struct ps_scgen *scgen) {
  if (!scgen) return;

  ps_scenario_del(scgen->scenario);
  if (scgen->msg) free(scgen->msg);
  if (scgen->screenbuf) free(scgen->screenbuf);
  ps_blueprint_list_del(scgen->blueprints);
  ps_blueprint_list_del(scgen->blueprints_scratch);
  ps_blueprint_list_del(scgen->blueprints_require);
  ps_zones_del(scgen->zones);

  free(scgen);
}

/* Set error message.
 */
 
int ps_scgen_failv(struct ps_scgen *scgen,const char *fmt,va_list vargs) {
  if (!scgen) return -1;
  if (fmt&&fmt[0]) {
    char buf[256];
    int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
    if ((bufc<0)||(bufc>=sizeof(buf))) bufc=0;
    if (bufc) {
      char *nv=malloc(bufc+1);
      if (nv) {
        memcpy(nv,buf,bufc+1);
        if (scgen->msg) free(scgen->msg);
        scgen->msg=nv;
        scgen->msgc=bufc;
      }
    }
  }
  return -1;
}

int ps_scgen_fail(struct ps_scgen *scgen,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return ps_scgen_failv(scgen,fmt,vargs);
}

#define FAIL_NULL(fmt,...) { ps_scgen_fail(scgen,fmt,##__VA_ARGS__); return 0; }

/* Add to screen buffer.
 */

static int ps_scgen_screenbuf_append(struct ps_scgen *scgen,struct ps_screen *screen) {
  if (!scgen||!screen) return -1;

  int i; for (i=scgen->screenbufc;i-->0;) if (scgen->screenbuf[i]==screen) return 0;
  
  if (scgen->screenbufc>=scgen->screenbufa) {
    int na=scgen->screenbufa+16;
    if (na>=INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(scgen->screenbuf,sizeof(void*)*na);
    if (!nv) return -1;
    scgen->screenbuf=nv;
    scgen->screenbufa=na;
  }

  scgen->screenbuf[scgen->screenbufc++]=screen;

  return 1;
}

/* Test screen buffer.
 */

static int ps_scgen_screenbuf_contains(struct ps_scgen *scgen,struct ps_screen *screen) {
  int i; for (i=scgen->screenbufc;i-->0;) if (scgen->screenbuf[i]==screen) return 1;
  return 0;
}

/* Random integer.
 * We wrap each call to rand(), in case it needs to be mocked out or something.
 */

int ps_scgen_randint(struct ps_scgen *scgen,int limit) {
  if (limit<1) return 0;
  return rand()%limit;
}

/* Randomize directions.
 * Return 0..3 in a random order.
 */

static void ps_scgen_assign_random_direction(int *dst,int dir,int vp) {
  int p=0; for (;p<4;p++) {
    if (dst[p]<0) {
      if (!vp--) {
        dst[p]=dir;
        return;
      }
    }
  }
}

static int ps_scgen_randomize_directions(int *dst,struct ps_scgen *scgen) {
  dst[0]=dst[1]=dst[2]=dst[3]=-1;
  int i=4; while (i>0) {
    int p=ps_scgen_randint(scgen,i--);
    ps_scgen_assign_random_direction(dst,i+1,p);
  }
  return 0;
}

/* Validate inputs.
 */

static int ps_scgen_validate(struct ps_scgen *scgen) {

  if ((scgen->playerc<1)||(scgen->playerc>PS_PLAYER_LIMIT)) {
    return ps_scgen_fail(scgen,"Invalid player count %d (1..%d).",scgen->playerc,PS_PLAYER_LIMIT);
  }

  if (!scgen->skills) {
    ps_log(GENERATOR,WARN,"Skill set zero -- this is highly improbable.");
  }

  if ((scgen->difficulty<PS_DIFFICULTY_MIN)||(scgen->difficulty>PS_DIFFICULTY_MAX)) {
    return ps_scgen_fail(scgen,"Invalid difficulty %d (%d..%d).",scgen->difficulty,PS_DIFFICULTY_MIN,PS_DIFFICULTY_MAX);
  }

  if ((scgen->length<PS_LENGTH_MIN)||(scgen->length>PS_LENGTH_MAX)) {
    return ps_scgen_fail(scgen,"Invalid length %d (%d..%d).",scgen->length,PS_LENGTH_MIN,PS_LENGTH_MAX);
  }

  return 0;
}

/* "Calculate" world size.
 * For now at least, we just use a table of constants.
 * Assume that each feature (treasurec+1) will require 5 screens. It's actually a lot less.
 */

static int ps_scgen_calculate_world_size(int *w,int *h,int *treasurec,struct ps_scgen *scgen) {
  switch (scgen->length) {
    case  1: *w= 3; *h= 4; *treasurec= 1; return 0; // 3*4=12, 2*5=10
    case  2: *w= 4; *h= 4; *treasurec= 2; return 0; // 4*4=16, 3*5=15
    case  3: *w= 5; *h= 4; *treasurec= 3; return 0; // 5*4=20, 4*5=20
    case  4: *w= 6; *h= 5; *treasurec= 5; return 0; // 6*5=30, 6*5=30
    case  5: *w= 6; *h= 6; *treasurec= 6; return 0; // 6*6=36, 7*5=35
    case  6: *w= 9; *h= 6; *treasurec= 9; return 0; // 9*6=54, 10*5=50
    case  7: *w= 9; *h= 8; *treasurec=12; return 0; // 9*8=72, 13*5=65
    case  8: *w=10; *h= 8; *treasurec=14; return 0; // 10*8=80, 15*5=75
    case  9: *w=12; *h= 9; *treasurec=16; return 0; // 12*9=108, 17*5=85
  }
  return ps_scgen_fail(scgen,"Invalid length %d.",scgen->length);
}

/* Select unused screens randomly with guidance.
 */

static struct ps_screen *ps_scgen_select_edge_screen(struct ps_scgen *scgen) {

  /* List all candidate screens. */
  #define CANDIDATE(x,y) { \
    struct ps_screen *screen=PS_SCENARIO_SCREEN(scgen->scenario,x,y); \
    if (!screen->features) { \
      if (ps_scgen_screenbuf_append(scgen,screen)<0) return 0; \
    } \
  }
  scgen->screenbufc=0;
  int w=scgen->scenario->w;
  int h=scgen->scenario->h;
  int x; for (x=0;x<w;x++) {
    CANDIDATE(x,0)
    CANDIDATE(x,h-1)
  }
  int y; for (y=0;y<h;y++) {
    CANDIDATE(0,y)
    CANDIDATE(w-1,y)
  }
  #undef CANDIDATE

  /* Select one randomly. */
  if (scgen->screenbufc<1) FAIL_NULL("Can't find an available edge screen.")
  int p=ps_scgen_randint(scgen,scgen->screenbufc);
  return scgen->screenbuf[p];
}

static struct ps_screen *ps_scgen_select_middle_screen(struct ps_scgen *scgen) {
  int w=scgen->scenario->w;
  int h=scgen->scenario->h;
  int x,y;

  /* Odd sizes are deterministic, for even sizes it's 50/50. */
  if (w&1) x=w>>1; else x=(w>>1)-ps_scgen_randint(scgen,2);
  if (h&1) y=h>>1; else y=(h>>1)-ps_scgen_randint(scgen,2);

  return PS_SCENARIO_SCREEN(scgen->scenario,x,y);
}

static struct ps_screen *ps_scgen_select_lonely_screen(struct ps_scgen *scgen) {
  int w=scgen->scenario->w;
  int h=scgen->scenario->h;
  int screenc=w*h;
  scgen->screenbufc=0;
  struct ps_screen *screen=scgen->scenario->screenv;
  int i=0; for (;i<screenc;i++,screen++) {
    if (screen->features) continue;
    if (screen->x>0) {
      struct ps_screen *neighbor=screen-1;
      if (neighbor->features) continue;
    }
    if (screen->y>0) {
      struct ps_screen *neighbor=screen-w;
      if (neighbor->features) continue;
    }
    if (screen->x<w-1) {
      struct ps_screen *neighbor=screen+1;
      if (neighbor->features) continue;
    }
    if (screen->y<h-1) {
      struct ps_screen *neighbor=screen+w;
      if (neighbor->features) continue;
    }
    if (ps_scgen_screenbuf_append(scgen,screen)<0) return 0;
  }
  if (scgen->screenbufc<1) FAIL_NULL("Unable to locate lonely screen for new feature (length=%d).",scgen->length)
  int p=ps_scgen_randint(scgen,scgen->screenbufc);
  return scgen->screenbuf[p];
}

/* In a brand-new world, decide which screens will be HOME and TREASURE.
 */

static int ps_scgen_place_home_and_treasure_screens(struct ps_scgen *scgen) {

  /* If there's only one treasure, HOME goes along an edge. Otherwise, in the middle. */
  struct ps_screen *screen_home;
  if (scgen->treasurec==1) screen_home=ps_scgen_select_edge_screen(scgen);
  else screen_home=ps_scgen_select_middle_screen(scgen);
  if (!screen_home) return -1;
  screen_home->features|=PS_SCREEN_FEATURE_HOME;
  scgen->homex=screen_home->x;
  scgen->homey=screen_home->y;
  scgen->scenario->homex=scgen->homex;
  scgen->scenario->homey=scgen->homey;

  /* Select a screen for each treasure. */
  int i; for (i=0;i<scgen->treasurec;i++) {
    struct ps_screen *screen_treasure=ps_scgen_select_lonely_screen(scgen);
    if (!screen_treasure) return -1;
    if (screen_treasure->features) return -1;
    screen_treasure->features|=PS_SCREEN_FEATURE_TREASURE;
  }

  return 0;
}

/* Test existance of a path based on screen doors (OPEN only).
 * We use screenbuf as a 'visited' list, you must clear it first.
 */

static int ps_scgen_path_exists(struct ps_scgen *scgen,struct ps_scenario *scenario,int ax,int ay,int bx,int by) {
  if ((ax==bx)&&(ay==by)) return 1;
  if ((ax<0)||(ax>=scenario->w)) return 0;
  if ((ay<0)||(ay>=scenario->h)) return 0;
  if ((bx<0)||(bx>=scenario->w)) return 0;
  if ((by<0)||(by>=scenario->h)) return 0;

  struct ps_screen *ascreen=PS_SCENARIO_SCREEN(scenario,ax,ay);
  if (ps_scgen_screenbuf_contains(scgen,ascreen)) return 0;
  if (ps_scgen_screenbuf_append(scgen,ascreen)<0) return 0;

  int screenbufc0=scgen->screenbufc;
  if (ascreen->doorn==PS_DOOR_OPEN) {
    if (ps_scgen_path_exists(scgen,scenario,ax,ay-1,bx,by)) return 1;
  }
  if (ascreen->doors==PS_DOOR_OPEN) {
    if (ps_scgen_path_exists(scgen,scenario,ax,ay+1,bx,by)) return 1;
  }
  if (ascreen->doorw==PS_DOOR_OPEN) {
    if (ps_scgen_path_exists(scgen,scenario,ax-1,ay,bx,by)) return 1;
  }
  if (ascreen->doore==PS_DOOR_OPEN) {
    if (ps_scgen_path_exists(scgen,scenario,ax+1,ay,bx,by)) return 1;
  }

  return 0;
}

/* Forge a path from the given screen to (dstx,dsty), recursive entry point.
 * (strict) prevents the path from running through another TREASURE screen.
 * Normally, this is the behavior we want.
 * However, that restriction can make certain layouts impossible.
 * So if the (strict) call fails, try again without (strict).
 *
 * Here's a real example of a failed situation, which would pass without (strict):
O?????????????OO?????????????OO?????????????OO?????????????OO             OO?????????????O
|        null           null           null           null           null           null |
|    TREASURE                                         HOME                      TREASURE |
|     unknown        unknown        unknown        unknown        unknown        unknown |
|           0              0              0              0              0              0 |
O?????????????OO?????????????OO             OO?????????????OO?????????????OO?????????????O
O?????????????OO?????????????OO             OO?????????????OO?????????????OO?????????????O
|        null ??        null           null ??        null ??        null ??        null |
|             ??    TREASURE                ??             ??             ??             |
|     unknown ??     unknown        unknown ??     unknown ??     unknown ??     unknown |
|           0 ??           0              0 ??           0 ??           0 ??           0 |
O?????????????OO?????????????OO?????????????OO?????????????OO?????????????OO?????????????O
O?????????????OO?????????????OO?????????????OO?????????????OO?????????????OO?????????????O
|        null ??        null ??        null ??        null ??        null ??        null |
| FROM HERE   ??             ??    TREASURE ??             ??             ??             |
|     unknown ??     unknown ??     unknown ??     unknown ??     unknown ??     unknown |
|           0 ??           0 ??           0 ??           0 ??           0 ??           0 |
O-------------OO-------------OO-------------OO-------------OO-------------OO-------------O
 */

static int ps_scgen_forge_partial_path(
  struct ps_scgen *scgen,struct ps_scenario *scenario,struct ps_screen *screen,int dstx,int dsty,int strict
) {

  /* If this is the target, succeed. */
  if ((screen->x==dstx)&&(screen->y==dsty)) return 0;

  /* If we've already been here, fail. */
  if (ps_scgen_screenbuf_contains(scgen,screen)) return -1;

  /* Add this screen to the stack. */
  if (ps_scgen_screenbuf_append(scgen,screen)<0) return -1;

  /* Put the four cardinal directions in order of preference, based on distance. */
  struct direction { int dx,dy; } directionv[4];
  int dx=dstx-screen->x; int adx=(dx<0)?-dx:dx;
  int dy=dsty-screen->y; int ady=(dy<0)?-dy:dy;
  if (adx<=ady) {
    if (dy<0) {
      directionv[0]=(struct direction){0,-1};
      directionv[1]=(struct direction){0,1};
    } else {
      directionv[0]=(struct direction){0,1};
      directionv[1]=(struct direction){0,-1};
    }
    if (dx<0) {
      directionv[2]=(struct direction){-1,0};
      directionv[3]=(struct direction){1,0};
    } else {
      directionv[2]=(struct direction){1,0};
      directionv[3]=(struct direction){-1,0};
    }
  } else {
    if (dx<0) {
      directionv[0]=(struct direction){-1,0};
      directionv[1]=(struct direction){1,0};
    } else {
      directionv[0]=(struct direction){1,0};
      directionv[1]=(struct direction){-1,0};
    }
    if (dy<0) {
      directionv[2]=(struct direction){0,-1};
      directionv[3]=(struct direction){0,1};
    } else {
      directionv[2]=(struct direction){0,1};
      directionv[3]=(struct direction){0,-1};
    }
  }

  /* Consider each possible direction. 
   * Don't walk through closed doors, and don't visit TREASURE screens.
   */
  int screenbufc0=scgen->screenbufc;
  int directioni=0; for (;directioni<4;directioni++) {
    dx=directionv[directioni].dx;
    dy=directionv[directioni].dy;
    if (dx<0) {
      if (screen->doorw==PS_DOOR_CLOSED) continue;
      if (strict&&(screen[-1].features&PS_SCREEN_FEATURE_TREASURE)) continue;
      if (ps_scgen_forge_partial_path(scgen,scenario,screen-1,dstx,dsty,strict)>=0) return 0;
    } else if (dx>0) {
      if (screen->doore==PS_DOOR_CLOSED) continue;
      if (strict&&(screen[1].features&PS_SCREEN_FEATURE_TREASURE)) continue;
      if (ps_scgen_forge_partial_path(scgen,scenario,screen+1,dstx,dsty,strict)>=0) return 0;
    } else if (dy<0) {
      if (screen->doorn==PS_DOOR_CLOSED) continue;
      if (strict&&(screen[-scenario->w].features&PS_SCREEN_FEATURE_TREASURE)) continue;
      if (ps_scgen_forge_partial_path(scgen,scenario,screen-scenario->w,dstx,dsty,strict)>=0) return 0;
    } else if (dy>0) {
      if (screen->doors==PS_DOOR_CLOSED) continue;
      if (strict&&(screen[scenario->w].features&PS_SCREEN_FEATURE_TREASURE)) continue;
      if (ps_scgen_forge_partial_path(scgen,scenario,screen+scenario->w,dstx,dsty,strict)>=0) return 0;
    }
    scgen->screenbufc=screenbufc0;
  }

  return -1;
}

/* Forge a single path from the given screen to (dstx,dsty).
 */

static int ps_scgen_forge_path(struct ps_scgen *scgen,struct ps_scenario *scenario,struct ps_screen *screen,int dstx,int dsty) {

  /* Are they already connected? */
  scgen->screenbufc=0;
  if (ps_scgen_path_exists(scgen,scenario,screen->x,screen->y,dstx,dsty)) return 0;

  /* Use screenbuf as a stack of visited screens.
   * We then walk recursively to the goal.
   */
  scgen->screenbufc=0;
  if (ps_scgen_forge_partial_path(scgen,scenario,screen,dstx,dsty,1)<0) {
    scgen->screenbufc=0;
    if (ps_scgen_forge_partial_path(scgen,scenario,screen,dstx,dsty,0)<0) {
      return ps_scgen_fail(scgen,"Failed to identify path from (%d,%d) to (%d,%d).",screen->x,screen->y,dstx,dsty);
    }
  }

  /* screenbuf now describes the path to goal. Follow it, opening doors along the way.
   */
  int i=0; for (;i<scgen->screenbufc;i++) {
    screen=scgen->screenbuf[i];
    int dx,dy;
    if (i<scgen->screenbufc-1) {
      struct ps_screen *next=scgen->screenbuf[i+1];
      dx=next->x-screen->x;
      dy=next->y-screen->y;
    } else {
      dx=dstx-screen->x;
      dy=dsty-screen->y;
    }
    if (dx<0) {
      screen->doorw=PS_DOOR_OPEN;
      screen[-1].doore=PS_DOOR_OPEN;
    } else if (dx>0) {
      screen->doore=PS_DOOR_OPEN;
      screen[1].doorw=PS_DOOR_OPEN;
    } else if (dy<0) {
      screen->doorn=PS_DOOR_OPEN;
      screen[-scenario->w].doors=PS_DOOR_OPEN;
    } else if (dy>0) {
      screen->doors=PS_DOOR_OPEN;
      screen[scenario->w].doorn=PS_DOOR_OPEN;
    }
  }

  return 0;
}

/* Change doors from UNSET to OPEN to ensure a path from each treasure to home.
 */

static int ps_scgen_forge_paths(struct ps_scgen *scgen) {
  struct ps_scenario *scenario=scgen->scenario;
  struct ps_screen *screen=scenario->screenv;
  int y=0; for (;y<scenario->h;y++) {
    int x=0; for (;x<scenario->w;x++,screen++) {
      if (!(screen->features&PS_SCREEN_FEATURE_TREASURE)) continue;
      if (ps_scgen_forge_path(scgen,scenario,screen,scgen->homex,scgen->homey)<0) return -1;
    }
  }
  return 0;
}

/* Open one door at random to promote reachability.
 */

static int ps_scgen_open_one_unset_door(struct ps_scgen *scgen,struct ps_screen *screen) {
  int optionv[4];
  int optionc=0;
  if (screen->doorn==PS_DOOR_UNSET) optionv[optionc++]=0;
  if (screen->doors==PS_DOOR_UNSET) optionv[optionc++]=1;
  if (screen->doorw==PS_DOOR_UNSET) optionv[optionc++]=2;
  if (screen->doore==PS_DOOR_UNSET) optionv[optionc++]=3;
  if (optionc<1) return 0;
  int which=optionv[ps_scgen_randint(scgen,optionc)];
  switch (which) {
    case 0: screen->doorn=PS_DOOR_OPEN; screen[-scgen->scenario->w].doors=PS_DOOR_OPEN; break;
    case 1: screen->doors=PS_DOOR_OPEN; screen[scgen->scenario->w].doorn=PS_DOOR_OPEN; break;
    case 2: screen->doorw=PS_DOOR_OPEN; screen[-1].doore=PS_DOOR_OPEN; break;
    case 3: screen->doore=PS_DOOR_OPEN; screen[1].doorw=PS_DOOR_OPEN; break;
  }
  return 1;
}

/* Find unreachable screens and force them to be reachable.
 * At this point, unreachable is easy to determine: all four doors will be CLOSED or UNSET.
 */

static int ps_scgen_make_all_screens_reachable(struct ps_scgen *scgen) {
  struct ps_scenario *scenario=scgen->scenario;

  /* Identify all unreachable screens, add them to screenbuf. */
  scgen->screenbufc=0;
  int i=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;i-->0;screen++) {
    if (screen->doorn==PS_DOOR_OPEN) continue;
    if (screen->doors==PS_DOOR_OPEN) continue;
    if (screen->doorw==PS_DOOR_OPEN) continue;
    if (screen->doore==PS_DOOR_OPEN) continue;
    if (ps_scgen_screenbuf_append(scgen,screen)<0) return -1;
  }
  if (scgen->screenbufc<1) return 0;

  /* Open one door at random in each unreachable screen.
   * This is not guaranteed to make them all reachable, but it's a great start.
   */
  for (i=0;i<scgen->screenbufc;i++) {
    if (ps_scgen_open_one_unset_door(scgen,scgen->screenbuf[i])<0) return -1;
  }

  /* Now it gets more expensive:
   * Test each of the identified screens for actual reachability.
   * If unreachable and an UNSET door exists, open it, then start the loop over.
   * ps_scgen_path_exists() uses screenbuf, so we must yoink it first.
   */
  struct ps_screen **unreachablev=scgen->screenbuf;
  int unreachablec=scgen->screenbufc;
  scgen->screenbuf=0;
  scgen->screenbufc=0;
  scgen->screenbufa=0;
  while (1) {
    int all_reachable=1;
    for (i=0;i<unreachablec;i++) {
      struct ps_screen *screen=unreachablev[i];
      scgen->screenbufc=0;
      if (ps_scgen_path_exists(scgen,scenario,screen->x,screen->y,scgen->homex,scgen->homey)) continue;
      int err=ps_scgen_open_one_unset_door(scgen,screen);
      if (err<0) {
        free(unreachablev);
        return -1;
      }
      if (err>0) {
        all_reachable=0;
        break;
      }
    }
    if (all_reachable) break;
  }
  free(unreachablev);

  return 0;
}

/* Turn UNSET doors into CLOSED.
 */

static int ps_scgen_finalize_doors(struct ps_scgen *scgen) {
  struct ps_scenario *scenario=scgen->scenario;
  struct ps_screen *screen=scenario->screenv;
  int i=scenario->w*scenario->h; for (;i-->0;screen++) {
    if (screen->doorn==PS_DOOR_UNSET) screen->doorn=PS_DOOR_CLOSED;
    if (screen->doors==PS_DOOR_UNSET) screen->doors=PS_DOOR_CLOSED;
    if (screen->doorw==PS_DOOR_UNSET) screen->doorw=PS_DOOR_CLOSED;
    if (screen->doore==PS_DOOR_UNSET) screen->doore=PS_DOOR_CLOSED;
  }
  return 0;
}

/* Identify homeward direction per screen.
 */

static int ps_scgen_identify_homeward_direction_1(struct ps_scgen *scgen,struct ps_screen *screen,int direction) {
  struct ps_scenario *scenario=scgen->scenario;

  int err=ps_scgen_screenbuf_append(scgen,screen);
  if (err<=0) return err;

  screen->direction_home=direction;

  if (screen->doorw==PS_DOOR_OPEN) {
    if (ps_scgen_identify_homeward_direction_1(scgen,screen-1,PS_DIRECTION_EAST)<0) return -1;
  }
  if (screen->doore==PS_DOOR_OPEN) {
    if (ps_scgen_identify_homeward_direction_1(scgen,screen+1,PS_DIRECTION_WEST)<0) return -1;
  }
  if (screen->doorn==PS_DOOR_OPEN) {
    if (ps_scgen_identify_homeward_direction_1(scgen,screen-scenario->w,PS_DIRECTION_SOUTH)<0) return -1;
  }
  if (screen->doors==PS_DOOR_OPEN) {
    if (ps_scgen_identify_homeward_direction_1(scgen,screen+scenario->w,PS_DIRECTION_NORTH)<0) return -1;
  }

  return 0;
}

static int ps_scgen_identify_homeward_directions(struct ps_scgen *scgen) {
  struct ps_screen *screen=PS_SCENARIO_SCREEN(scgen->scenario,scgen->homex,scgen->homey);
  scgen->screenbufc=0;
  if (ps_scgen_identify_homeward_direction_1(scgen,screen,0)<0) return -1;
  return 0;
}

/* Move treasure screens away from home.
 */

static int ps_scgen_move_treasure_awayward(struct ps_scgen *scgen,struct ps_screen *screen,int panic) {

  /* It is possible for loops to form, and we could end up circling within them.
   * This whole operation is best-effort anyway, so just break them.
   */
  if (--panic<=0) return 0;

  int direction=ps_direction_reverse(screen->direction_home);

  struct ps_screen *neighbor=ps_screen_neighbor_for_direction(screen,direction,scgen->scenario->w);
  if (!neighbor||neighbor->features) {
    neighbor=ps_screen_neighbor_for_direction(screen,ps_direction_rotate_clockwise(direction),scgen->scenario->w);
    if (neighbor&&!neighbor->features) {
      direction=ps_direction_rotate_clockwise(direction);
    } else {
      neighbor=ps_screen_neighbor_for_direction(screen,ps_direction_rotate_counterclockwise(direction),scgen->scenario->w);
      if (neighbor&&!neighbor->features) {
        direction=ps_direction_rotate_counterclockwise(direction);
      } else {
        return 0;
      }
    }
  }

  neighbor->features|=PS_SCREEN_FEATURE_TREASURE;
  screen->features&=~PS_SCREEN_FEATURE_TREASURE;
  
  return ps_scgen_move_treasure_awayward(scgen,neighbor,panic);
}

static int ps_scgen_move_treasures_awayward(struct ps_scgen *scgen) {
  struct ps_screen *screen=scgen->scenario->screenv;
  int i=scgen->scenario->w*scgen->scenario->h;
  for (;i-->0;screen++) {
    if (!(screen->features&PS_SCREEN_FEATURE_TREASURE)) continue;
    if (ps_scgen_move_treasure_awayward(scgen,screen,20)<0) return -1;
  }
  return 0;
}

/* Identify prime challenges.
 * These are featureless screens with two doors adjacent to a treasure.
 */

static int ps_scgen_identify_prime_challenges(struct ps_scgen *scgen) {
  struct ps_screen *screen=scgen->scenario->screenv;
  int i=scgen->scenario->w*scgen->scenario->h;
  for (;i-->0;screen++) {
    if (!(screen->features&PS_SCREEN_FEATURE_TREASURE)) continue;
    struct ps_screen *neighbor=ps_screen_get_single_neighbor(screen,scgen->scenario->w);
    if (!neighbor) continue;
    if (neighbor->features) continue;
    if (ps_screen_count_doors(neighbor)!=2) continue;
    neighbor->features|=PS_SCREEN_FEATURE_CHALLENGE;
  }
  return 0;
}

/* Blueprint filters.
 */

static int ps_scgen_blueprint_filter_home(const struct ps_blueprint *blueprint) {
  if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_HERO)>0) return 1;
  return 0;
}

static int ps_scgen_blueprint_filter_treasure(const struct ps_blueprint *blueprint) {
  if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_TREASURE)>0) return 1;
  return 0;
}

static int ps_scgen_blueprint_filter_challenge(const struct ps_blueprint *blueprint) {
  if (blueprint->solutionc>0) return 1;
  return 0;
}

static int ps_scgen_blueprint_filter_other(const struct ps_blueprint *blueprint) {
  if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_HERO)>0) return 0;
  if (ps_blueprint_count_poi_of_type(blueprint,PS_BLUEPRINT_POI_TREASURE)>0) return 0;
  if (blueprint->solutionc>0) return 0;
  return 1;
}

/* Assign blueprint and xform in each screen.
 */

static int ps_scgen_select_blueprints(struct ps_scgen *scgen) {
  struct ps_scenario *scenario=scgen->scenario;

  /* Build the global set of legal blueprints. */
  if (!scgen->blueprints) {
    if (!(scgen->blueprints=ps_blueprint_list_new())) return -1;
    if (ps_blueprint_list_match_resources(
      scgen->blueprints,scgen->playerc,scgen->skills,scgen->difficulty
    )<0) return -1;
  }

  ps_log(GENERATOR,DEBUG,"Have %d legal blueprints globally.",scgen->blueprints->c);

  /* Classify all blueprints and count those of each class. */
  int bpc_home=ps_blueprint_list_count_by_filter(scgen->blueprints,ps_scgen_blueprint_filter_home);
  int bpc_treasure=ps_blueprint_list_count_by_filter(scgen->blueprints,ps_scgen_blueprint_filter_treasure);
  if (bpc_home<1) return ps_scgen_fail(scgen,"No acceptable home blueprints found.");
  if (bpc_treasure<1) return ps_scgen_fail(scgen,"No acceptable treasure blueprints found.");
  
  /* Make two blueprint lists, for challenges and "others". */
  struct ps_blueprint_list *challenges=ps_blueprint_list_new();
  if (!challenges) return -1;
  struct ps_blueprint_list *others=ps_blueprint_list_new();
  if (!others) {
    ps_blueprint_list_del(challenges);
    return -1;
  }
  if (
    (ps_blueprint_list_apply_filter(challenges,scgen->blueprints,ps_scgen_blueprint_filter_challenge)<0)||
    (ps_blueprint_list_apply_filter(others,scgen->blueprints,ps_scgen_blueprint_filter_other)<0)
  ) {
    ps_blueprint_list_del(challenges);
    ps_blueprint_list_del(others);
    return -1;
  }

  ps_log(GENERATOR,DEBUG,"Classified blueprints: home=%d, treasure=%d, challenge=%d, other=%d",bpc_home,bpc_treasure,challenges->c,others->c);

  /* Grab randomly from the available blueprints for each screen. */
  struct ps_screen *screen=scenario->screenv;
  int i=scenario->w*scenario->h;
  for (;i-->0;screen++) {
    struct ps_blueprint *blueprint=0;
    
    if (screen->features&PS_SCREEN_FEATURE_HOME) {
      int p=ps_scgen_randint(scgen,bpc_home);
      blueprint=ps_blueprint_list_get_by_filter(scgen->blueprints,ps_scgen_blueprint_filter_home,p);

    } else if (screen->features&PS_SCREEN_FEATURE_TREASURE) {
      int p=ps_scgen_randint(scgen,bpc_treasure);
      blueprint=ps_blueprint_list_get_by_filter(scgen->blueprints,ps_scgen_blueprint_filter_treasure,p);

    } else if (screen->features&PS_SCREEN_FEATURE_CHALLENGE) {
      if (challenges->c>0) {
        int p=ps_scgen_randint(scgen,challenges->c);
        blueprint=challenges->v[p];
        ps_blueprint_list_remove_at(challenges,p);
      } else if (others->c>0) {
        int p=ps_scgen_randint(scgen,others->c);
        blueprint=others->v[p];
        ps_blueprint_list_remove_at(others,p);
      } else {
        int p=ps_scgen_randint(scgen,scgen->blueprints->c);
        blueprint=scgen->blueprints->v[p];
      }

    } else {
      if (others->c>0) {
        int p=ps_scgen_randint(scgen,others->c);
        blueprint=others->v[p];
        ps_blueprint_list_remove_at(others,p);
      } else if (challenges->c>0) {
        int p=ps_scgen_randint(scgen,challenges->c);
        blueprint=challenges->v[p];
        ps_blueprint_list_remove_at(challenges,p);
      } else {
        int p=ps_scgen_randint(scgen,scgen->blueprints->c);
        blueprint=scgen->blueprints->v[p];
      }

    }
    if (!blueprint||(ps_screen_set_blueprint(screen,blueprint)<0)) {
      ps_blueprint_list_del(challenges);
      ps_blueprint_list_del(others);
      return -1;
    }

    /* If the blueprint has a challenge, set xform based on direction_home. */
    if (blueprint->solutionc>0) {
      int single_awayward_direction=ps_screen_get_single_awayward_direction(screen);
      switch (screen->direction_home) {
        #define FLIP_XFORM_COIN(a,b) (ps_scgen_randint(scgen,2)?PS_BLUEPRINT_XFORM_##a:PS_BLUEPRINT_XFORM_##b)
        case PS_DIRECTION_NORTH: switch (single_awayward_direction) {
            case PS_DIRECTION_EAST: screen->xform=PS_BLUEPRINT_XFORM_NONE; break;
            case PS_DIRECTION_WEST: screen->xform=PS_BLUEPRINT_XFORM_HORZ; break;
            default: screen->xform=FLIP_XFORM_COIN(NONE,HORZ); break;
          } break;
        case PS_DIRECTION_SOUTH: switch (single_awayward_direction) {
            case PS_DIRECTION_EAST: screen->xform=PS_BLUEPRINT_XFORM_VERT; break;
            case PS_DIRECTION_WEST: screen->xform=PS_BLUEPRINT_XFORM_BOTH; break;
            default: screen->xform=FLIP_XFORM_COIN(VERT,BOTH); break;
          } break;
        case PS_DIRECTION_WEST: switch (single_awayward_direction) {
            case PS_DIRECTION_NORTH: screen->xform=PS_BLUEPRINT_XFORM_VERT; break;
            case PS_DIRECTION_SOUTH: screen->xform=PS_BLUEPRINT_XFORM_NONE; break;
            default: screen->xform=FLIP_XFORM_COIN(VERT,NONE); break;
          } break;
        case PS_DIRECTION_EAST: switch (single_awayward_direction) {
            case PS_DIRECTION_NORTH: screen->xform=PS_BLUEPRINT_XFORM_BOTH; break;
            case PS_DIRECTION_SOUTH: screen->xform=PS_BLUEPRINT_XFORM_HORZ; break;
            default: screen->xform=FLIP_XFORM_COIN(BOTH,HORZ); break;
          } break;
        #undef FLIP_XFORM_COIN
      }
      
    /* Without a challenge, set a random xform, for aesthetic purposes only. */
    } else {
      screen->xform=ps_scgen_randint(scgen,4);
    }

    /* Generate the initial grid for screen. */
    if (ps_screen_build_inner_grid(screen)<0) {
      ps_blueprint_list_del(challenges);
      ps_blueprint_list_del(others);
      return -1;
    }
  }

  ps_blueprint_list_del(challenges);
  ps_blueprint_list_del(others);
  return 0;
}

/* Populate grid margins.
 */

static int ps_scgen_populate_grid_margins(struct ps_scgen *scgen) {
  struct ps_scenario *scenario=scgen->scenario;
  int i=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;i-->0;screen++) {
    struct ps_screen *neighborn=ps_screen_neighbor_for_direction(screen,PS_DIRECTION_NORTH,scenario->w);
    struct ps_screen *neighbors=ps_screen_neighbor_for_direction(screen,PS_DIRECTION_SOUTH,scenario->w);
    struct ps_screen *neighborw=ps_screen_neighbor_for_direction(screen,PS_DIRECTION_WEST,scenario->w);
    struct ps_screen *neighbore=ps_screen_neighbor_for_direction(screen,PS_DIRECTION_EAST,scenario->w);
    if (ps_screen_populate_grid_margins(screen,
      neighborn?neighborn->grid:0,neighbors?neighbors->grid:0,
      neighborw?neighborw->grid:0,neighbore?neighbore->grid:0
    )<0) return -1;
  }
  return 0;
}

/* Given a screen with no region, how many regionless screens are contiguously attached?
 * Uses and resets screen->visited.
 * You may provide a limit to improve performance.
 * This will give bad counts if there is a loop, due to resetting screen->visited after each step.
 * That doesn't come up very often, and does no real harm when it does.
 * We could fix it with an external list of visited screens, but I don't think that that's worthwhile.
 */

static int ps_scgen_count_unassigned_regions(struct ps_scgen *scgen,struct ps_screen *screen,int limit) {
  if (screen->region) return 0;
  if (screen->visited) return 0;
  if (limit<=1) return 1;
  screen->visited=1;
  int c=1;
  int dir=1; for (;dir<=4;dir++) {
    struct ps_screen *neighbor=ps_screen_neighbor_for_direction(screen,dir,scgen->scenario->w);
    if (!neighbor) continue;
    c+=ps_scgen_count_unassigned_regions(scgen,neighbor,limit-c);
    if (c>=limit) break;
  }
  screen->visited=0;
  return c;
}

/* Assign the region of this screen to neighboring screens, in a seed-fill fashion.
 * Return the count of screens changed.
 */

static int ps_scgen_expand_region(struct ps_scgen *scgen,struct ps_screen *screen,int c) {
  if (c<1) return 0;

  int i,err,result=0;
  int dirv[4];
  ps_scgen_randomize_directions(dirv,scgen);
  int availablev[4]={0};
  int availablesum=0;

  /* Calculate how far we can go in each direction. */
  for (i=0;i<4;i++) {
    struct ps_screen *neighbor=ps_screen_neighbor_for_direction(screen,dirv[i],scgen->scenario->w);
    if (!neighbor) continue;
    if (neighbor->region) continue;
    availablev[i]=ps_scgen_count_unassigned_regions(scgen,neighbor,c);
    availablesum+=availablev[i];
  }
  if (!availablesum) return 0;

  /* Decide how many in each direction we want to consume. */
  if (c>availablesum) c=availablesum;
  int usev[4]={0};
  int usesum=0;
  while (usesum<c) {
    for (i=0;i<4;i++) {
      if (availablev[i]>0) {
        availablev[i]--;
        usev[i]++;
        usesum++;
        if (usesum>=c) break;
      }
    }
  }

  /* Recur into selected neighbors with our artificial limits. Gather the actual assigned count. */
  for (i=0;i<4;i++) {
    if (!usev[i]) continue;
    struct ps_screen *neighbor=ps_screen_neighbor_for_direction(screen,dirv[i],scgen->scenario->w);
    if (!neighbor) continue;
    if (neighbor->region) continue;
    result++;
    err=ps_scgen_expand_region(scgen,neighbor,usev[i]-1);
    result+=err;
  }

  return result;
}

/* Randomly select a region not currently in use.
 */

static int ps_scgen_region_in_use(struct ps_scgen *scgen,struct ps_region *region) {
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) {
    if (screen->region==region) return 1;
  }
  return 0;
}

static struct ps_region *ps_scgen_select_random_unused_region(struct ps_scgen *scgen,struct ps_restype *restype) {
  int panic=1000;
  while (panic-->0) {
    int p=ps_scgen_randint(scgen,restype->resc);
    struct ps_region *region=restype->resv[p].obj;
    if (ps_scgen_region_in_use(scgen,region)) continue;
    return region;
  }
  // Couldn't find one? Whatever, just return anything.
  return restype->resv[ps_scgen_randint(scgen,restype->resc)].obj;
}

/* Randomly select a screen with no region.
 */

static struct ps_screen *ps_scgen_select_random_regionless_screen(struct ps_scgen *scgen) {
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen;
  int regionlessc=0,i;
  for (i=screenc,screen=scgen->scenario->screenv;i-->0;screen++) {
    if (!screen->region) regionlessc++;
  }
  if (!regionlessc) return 0;
  int p=ps_scgen_randint(scgen,regionlessc);
  for (i=screenc,screen=scgen->scenario->screenv;i-->0;screen++) {
    if (screen->region) continue;
    if (p-->0) continue;
    return screen;
  }
  return 0;
}

/* Fill any regionless screens by copying its neighbor.
 * Return <0 for real errors, 0 if something was left unassigned, or >0 if complete.
 * If we return zero, caller will repeat.
 */

static int ps_scgen_expand_all_regions(struct ps_scgen *scgen) {
  int result=1;
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) {
    if (screen->region) continue;
    int dirv[4];
    ps_scgen_randomize_directions(dirv,scgen);
    int i=0; for (;i<4;i++) {
      struct ps_screen *neighbor=ps_screen_neighbor_for_direction(screen,dirv[i],scgen->scenario->w);
      if (!neighbor) continue;
      if (neighbor->region) {
        screen->region=neighbor->region;
        break;
      }
    }
    if (!screen->region) result=0;
  }
  return result;
}

/* Select a region for each screen.
 */

static int ps_scgen_select_regions(struct ps_scgen *scgen) {

  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_REGION);
  if (!restype) return -1;
  if (restype->resc<1) return ps_scgen_fail(scgen,"No region resources.");

  int regionc=restype->resc;
  int screenc=scgen->scenario->w*scgen->scenario->h;
  if (regionc>screenc) regionc=screenc;
  int regionsize=screenc/regionc;
  if (regionsize<1) regionsize=1;

  ps_log(GENERATOR,DEBUG,
    "Have %d regions total for %d screens. Will use %d regions, size of each about %d screens.",
    restype->resc,screenc,regionc,regionsize
  );

  /* Select a region at random for the home screen. */
  int homeregionp=ps_scgen_randint(scgen,restype->resc);
  struct ps_screen *screen=PS_SCENARIO_SCREEN(scgen->scenario,scgen->homex,scgen->homey);
  screen->region=restype->resv[homeregionp].obj;

  /* Seed fill from home for the calculated region size. */
  if (ps_scgen_expand_region(scgen,screen,regionsize-1)<0) return -1;

  /* For all remaining regions, select a random unvisited screen, pick a random unused region, and seed fill it. */
  int i; for (i=1;i<regionc;i++) {
    struct ps_region *region=ps_scgen_select_random_unused_region(scgen,restype);
    if (!region) break;
    screen=ps_scgen_select_random_regionless_screen(scgen);
    if (!screen) break;
    screen->region=region;
    if (ps_scgen_expand_region(scgen,screen,regionsize-1)<0) return -1;
  }

  /* There can still be gaps, and probably are. Walk the world until all screens have been assigned a region. */
  int panic=1000;
  while (1) {
    int err=ps_scgen_expand_all_regions(scgen);
    if (err) return err;
    if (--panic<0) {
      ps_log(GENERATOR,ERROR,"Failed to expand regions!");
      return 0;
    }
  }
}

/* Reset (scenario->treasurec,treasurev).
 * Select trdef resources from the resource manager.
 */

static struct ps_res_trdef *ps_scgen_select_treasure_resource(const struct ps_restype *restype,struct ps_res_trdef **alreadyv,int alreadyc) {
  while (1) {
    int p=rand()%restype->resc;
    struct ps_res_trdef *trdef=restype->resv[p].obj;
    int havec=0,i=alreadyc;
    while (i-->0) if (alreadyv[i]==trdef) havec++;
    if (havec>alreadyc/restype->resc) continue; // Don't reuse a treasure until all have been selected, evenly
    return trdef;
  }
}

static int ps_scgen_select_treasure_resources(struct ps_scgen *scgen) {

  struct ps_res_trdef **nv=calloc(sizeof(void*),scgen->treasurec);
  if (!nv) return -1;
  if (scgen->scenario->treasurev) free(scgen->scenario->treasurev);
  scgen->scenario->treasurev=nv;
  scgen->scenario->treasurec=scgen->treasurec;

  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_TRDEF);
  if (!restype) return -1;
  if (restype->resc<1) {
    ps_log(GENERATOR,ERROR,"No trdef resources found.");
    return -1;
  }

  int i=0; for (;i<scgen->treasurec;i++) {
    if (!(scgen->scenario->treasurev[i]=ps_scgen_select_treasure_resource(
      restype,scgen->scenario->treasurev,i
    ))) return -1;
  }

  return 0;
}

/* Assign treasureid in the grid POI, and confirm that we have the correct count of treasures.
 */

static int ps_scgen_eliminate_treasure_poi(struct ps_scgen *scgen,struct ps_grid *grid) {
  int i; for (i=0;i<grid->poic;) {
    struct ps_blueprint_poi *poi=grid->poiv+i;
    if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
      grid->poic--;
      memmove(poi,poi+1,sizeof(struct ps_blueprint_poi)*(grid->poic-i));
    } else i++;
  }
  return 0;
}

static int ps_scgen_assign_treasure_ids_in_grid(struct ps_scgen *scgen,struct ps_grid *grid,int treasureid) {
  int addc=0;
  struct ps_blueprint_poi *poi=grid->poiv;
  int i=grid->poic;
  for (;i-->0;poi++) {
    if (poi->type==PS_BLUEPRINT_POI_TREASURE) {
      poi->argv[0]=treasureid;
      treasureid++;
      addc++;
    }
  }
  return addc;
}

static int ps_scgen_assign_treasure_ids(struct ps_scgen *scgen) {
  int treasureid=0;
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) {
    if (!(screen->features&PS_SCREEN_FEATURE_TREASURE)) {
      if (ps_scgen_eliminate_treasure_poi(scgen,screen->grid)<0) return -1;
    } else {
      int addc=ps_scgen_assign_treasure_ids_in_grid(scgen,screen->grid,treasureid);
      if (addc<0) return -1;
      treasureid+=addc;
    }
  }
  if (treasureid!=scgen->treasurec) {
    return ps_scgen_fail(scgen,"Expected to produce %d treasures, actually produced %d.",scgen->treasurec,treasureid);
  }
  return 0;
}

/* Generate scenario, main entry point.
 */

int ps_scgen_generate(struct ps_scgen *scgen) {
  int64_t starttime=ps_time_now();
  if (!scgen) return -1;

  /* Ensure our inputs are valid. */
  if (ps_scgen_validate(scgen)<0) return -1;

  /* Create a new blank scenario. */
  if (scgen->scenario) {
    ps_scenario_del(scgen->scenario);
    scgen->scenario=0;
  }
  if (!(scgen->scenario=ps_scenario_new())) return -1;

  /* Create a blank world map. */
  int w,h;
  if (ps_scgen_calculate_world_size(&w,&h,&scgen->treasurec,scgen)<0) return -1;
  if ((scgen->treasurec<1)||(scgen->treasurec>PS_TREASURE_LIMIT)) return ps_scgen_fail(scgen,"treasurec=%d",scgen->treasurec);
  if (ps_scgen_select_treasure_resources(scgen)<0) return -1;
  if (ps_scenario_reallocate_screens(scgen->scenario,w,h)<0) return -1;

  /* Place the home and treasure screens. */
  if (ps_scgen_place_home_and_treasure_screens(scgen)<0) return -1;

  /* Forge a path to home from each treasure screen. */
  if (ps_scgen_forge_paths(scgen)<0) return -1;

  /* Make all screens reachable.
   * For now, we always do this. It could be dependant on a given difficulty level.
   * This step makes worlds larger by adding dead ends to them.
   */
  if (ps_scgen_make_all_screens_reachable(scgen)<0) return -1;

  /* Finalize doors: Anything still UNSET becomes CLOSED. */
  if (ps_scgen_finalize_doors(scgen)<0) return -1;

  /* Identify the homeward direction of each screen. */
  if (ps_scgen_identify_homeward_directions(scgen)<0) return -1;

  /* Adjust treasures to push them awayward from home. */
  if (ps_scgen_move_treasures_awayward(scgen)<0) return -1;

  /* Identify prime challenges. */
  if (ps_scgen_identify_prime_challenges(scgen)<0) return -1;

  /* Select blueprint and transform for each screen. */
  if (ps_scgen_select_blueprints(scgen)<0) return -1;

  /* Populate grid margins. */
  if (ps_scgen_populate_grid_margins(scgen)<0) return -1;

  /* Divide screens into thematic regions. */
  if (ps_scgen_select_regions(scgen)<0) return -1;

  /* Skin grids with graphics. */
  if (ps_scgen_generate_grids(scgen)<0) return -1;

  /* Assign treasure IDs and assert that all got assigned. */
  if (ps_scgen_assign_treasure_ids(scgen)<0) return -1;

  int64_t elapsed=ps_time_now()-starttime;
  ps_log(GENERATOR,INFO,"Generated scenario in %d.%06d s.",(int)(elapsed/1000000),(int)(elapsed%1000000));

  return 0;
}

/* (test) Select a blueprint for each screen from (blueprints_require).
 * We leave screen xforms untouched.
 * We do not necessarily produce a valid world map. This is for testing.
 */

static struct ps_blueprint *ps_scgen_choose_home_blueprint(struct ps_blueprint **srcv,int srcc) {
  int i; for (i=0;i<srcc;i++) {
    struct ps_blueprint *blueprint=srcv[i];
    int j; for (j=0;j<blueprint->poic;j++) {
      if (blueprint->poiv[j].type==PS_BLUEPRINT_POI_HERO) return blueprint;
    }
  }
  return 0;
}

static int ps_scgen_select_test_blueprints(struct ps_scgen *scgen) {
  if (!scgen->blueprints_require||(scgen->blueprints_require->c<1)) {
    return ps_scgen_fail(scgen,"No blueprints selected for test generator.");
  }

  /* The HOME screen is special, but everything else we will assign willy-nilly. */
  struct ps_blueprint *bphome=ps_scgen_choose_home_blueprint(scgen->blueprints_require->v,scgen->blueprints_require->c);
  if (!bphome) return ps_scgen_fail(scgen,"No home blueprint selected for test generator.");
  if (scgen->blueprints_require->c>1) { // If there's more than one, don't reuse the HOME blueprint.
    if (ps_blueprint_list_remove(scgen->blueprints_require,bphome)<0) return -1;
  }

  int bpp=0;
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) {
    if (screen->features&PS_SCREEN_FEATURE_HOME) {
      if (ps_screen_set_blueprint(screen,bphome)<0) return -1;
    } else {
      if (ps_screen_set_blueprint(screen,scgen->blueprints_require->v[bpp])<0) return -1;
      if (++bpp>=scgen->blueprints_require->c) bpp=0;
    }
    if (ps_screen_build_inner_grid(screen)<0) return -1;
  }

  return 0;
}

/* (test) Assign one region to every screen.
 */

static int ps_scgen_assign_region_to_all_screens(struct ps_scgen *scgen,int regionid) {
  struct ps_region *region=ps_res_get(PS_RESTYPE_REGION,regionid);
  if (!region) return ps_scgen_fail(scgen,"region:%d not found",regionid);
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) screen->region=region;
  return 0;
}

/* (test) Select a random region for each screen.
 */

static int ps_scgen_select_random_regions(struct ps_scgen *scgen) {
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_REGION);
  if (restype->resc<1) return -1;
  int i=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;i-->0;screen++) {
    int selection=ps_scgen_randint(scgen,restype->resc);
    screen->region=restype->resv[selection].obj;
  }
  return 0;
}

/* (test) Set screen doors such that all boundaries are open.
 */

static int ps_scgen_make_wide_open_doors(struct ps_scgen *scgen) {
  int screenc=scgen->scenario->w*scgen->scenario->h;
  struct ps_screen *screen=scgen->scenario->screenv;
  for (;screenc-->0;screen++) {
    screen->doorw=(screen->x?PS_DOOR_OPEN:PS_DOOR_CLOSED);
    screen->doorn=(screen->y?PS_DOOR_OPEN:PS_DOOR_CLOSED);
    screen->doore=((screen->x<scgen->scenario->w-1)?PS_DOOR_OPEN:PS_DOOR_CLOSED);
    screen->doors=((screen->y<scgen->scenario->h-1)?PS_DOOR_OPEN:PS_DOOR_CLOSED);
  }
  return 0;
}

/* Special interface for testing.
 */
 
int ps_scgen_test_require_blueprint(struct ps_scgen *scgen,int blueprintid) {
  if (!scgen) return -1;
  struct ps_blueprint *blueprint=ps_res_get(PS_RESTYPE_BLUEPRINT,blueprintid);
  if (!blueprint) {
    ps_log(GENERATOR,ERROR,"blueprint:%d not found",blueprintid);
    return -1;
  }
  if (!scgen->blueprints_require) {
    if (!(scgen->blueprints_require=ps_blueprint_list_new())) return -1;
  }
  if (ps_blueprint_list_add(scgen->blueprints_require,blueprint)<0) return -1;
  return 0;
}

int ps_scgen_test_require_region(struct ps_scgen *scgen,int regionid) {
  if (!scgen) return -1;
  if (!ps_res_get(PS_RESTYPE_REGION,regionid)) {
    ps_log(GENERATOR,ERROR,"region:%d not found",regionid);
    return -1;
  }
  scgen->regionid=regionid;
  return 0;
}

int ps_scgen_test_generate(struct ps_scgen *scgen) {
  int64_t starttime=ps_time_now();
  if (!scgen) return -1;

  /* Ensure our inputs are valid. */
  if (ps_scgen_validate(scgen)<0) return -1;

  /* Create a new blank scenario. */
  if (scgen->scenario) {
    ps_scenario_del(scgen->scenario);
    scgen->scenario=0;
  }
  if (!(scgen->scenario=ps_scenario_new())) return -1;

  /* Create a blank world map. */
  int w,h;
  if (ps_scgen_calculate_world_size(&w,&h,&scgen->treasurec,scgen)<0) return -1;
  if (ps_scenario_reallocate_screens(scgen->scenario,w,h)<0) return -1;

  /* Place the home and treasure screens. */
  if (ps_scgen_place_home_and_treasure_screens(scgen)<0) return -1;

  /* Make doors between all screens. */
  if (ps_scgen_make_wide_open_doors(scgen)<0) return -1;

  /* Select blueprint and transform for each screen. */
  if (ps_scgen_select_test_blueprints(scgen)<0) return -1;

  /* Populate grid margins. */
  if (ps_scgen_populate_grid_margins(scgen)<0) return -1;

  /* Divide screens into thematic regions. */
  if (scgen->regionid>=0) {
    if (ps_scgen_assign_region_to_all_screens(scgen,scgen->regionid)<0) return -1;
  } else {
    if (ps_scgen_select_random_regions(scgen)<0) return -1;
  }

  /* Skin grids with graphics. */
  if (ps_scgen_generate_grids(scgen)<0) return -1;

  int64_t elapsed=ps_time_now()-starttime;
  ps_log(GENERATOR,INFO,"Generated scenario in %d.%06d s.",(int)(elapsed/1000000),(int)(elapsed%1000000));

  return 0;
}
