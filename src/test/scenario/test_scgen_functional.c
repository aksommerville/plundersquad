#include "test/ps_test.h"
#include "res/ps_resmgr.h"
#include "res/ps_restype.h"
#include "scenario/ps_scgen.h"
#include "scenario/ps_scenario.h"
#include "scenario/ps_screen.h"
#include "scenario/ps_blueprint.h"
#include "scenario/ps_grid.h"
#include "scenario/ps_region.h"
#include "akpng/akpng.h"
#include "akgl/akgl.h"
#include "os/ps_fs.h"
#include <time.h>

/* Dump scenario.
 */

static char describe_blueprint_buffer[16];

static int id_for_blueprint(const struct ps_blueprint *blueprint) {
  const struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_BLUEPRINT);
  if (!restype) return -1;
  struct ps_res *res=restype->resv;
  int i; for (i=0;i<restype->resc;i++,res++) {
    if (res->obj==blueprint) return res->id;
  }
  return -1;
}

static const char *describe_blueprint(const struct ps_screen *screen) {
  if (!screen->blueprint) return "null";
  int id=id_for_blueprint(screen->blueprint);
  const char *xform=0;
  switch (screen->xform) {
    case PS_BLUEPRINT_XFORM_NONE: xform="NONE"; break;
    case PS_BLUEPRINT_XFORM_HORZ: xform="HORZ"; break;
    case PS_BLUEPRINT_XFORM_VERT: xform="VERT"; break;
    case PS_BLUEPRINT_XFORM_BOTH: xform="BOTH"; break;
  }
  snprintf(describe_blueprint_buffer,16,"%d:%s",id,xform);
  return describe_blueprint_buffer;
}

static char describe_screen_features_buffer[16];

static const char *describe_screen_features(uint8_t features) {
  switch (features) {
    case 0: return "";
    case PS_SCREEN_FEATURE_HOME: return "HOME";
    case PS_SCREEN_FEATURE_TREASURE: return "TREASURE";
    case PS_SCREEN_FEATURE_CHALLENGE: return "CHALLENGE";
  }
  snprintf(describe_screen_features_buffer,16,"0x%02x",features);
  return describe_screen_features_buffer;
}

static char ldoor(const struct ps_screen *screen) {
  switch (screen->doorw) {
    case PS_DOOR_UNSET: return '?';
    case PS_DOOR_CLOSED: return '|';
    case PS_DOOR_OPEN: return ' ';
    default: return '!';
  }
}

static char rdoor(const struct ps_screen *screen) {
  switch (screen->doore) {
    case PS_DOOR_UNSET: return '?';
    case PS_DOOR_CLOSED: return '|';
    case PS_DOOR_OPEN: return ' ';
    default: return '!';
  }
}

static int screen_unreachable(const struct ps_screen *screen) {
  if (screen->doorn!=PS_DOOR_CLOSED) return 0;
  if (screen->doors!=PS_DOOR_CLOSED) return 0;
  if (screen->doorw!=PS_DOOR_CLOSED) return 0;
  if (screen->doore!=PS_DOOR_CLOSED) return 0;
  return 1;
}

static void dump_world(struct ps_scenario *scenario) {
  PS_LOG("----- world -----")
  PS_LOG("Size %d,%d",scenario->w,scenario->h)
  struct ps_screen *screenrow=scenario->screenv;
  int y=0; for (;y<scenario->h;y++,screenrow+=scenario->w) {
    int x;
    struct ps_screen *screen;
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) switch (screen->doorn) {
      case PS_DOOR_UNSET:  printf("O?????????????O"); break;
      case PS_DOOR_CLOSED: printf("O-------------O"); break;
      case PS_DOOR_OPEN:   printf("O             O"); break;
      default:             printf("O!!!!! %02x !!!!O",screen->doorn); break;
    }
    printf("\n");
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) {
      if (screen_unreachable(screen)) {
        printf("|XXXXXXXXXXXXX|");
      } else {
        printf("%c %11s %c",ldoor(screen),describe_blueprint(screen),rdoor(screen));
      }
    }
    printf("\n");
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) {
      if (screen_unreachable(screen)) {
        printf("|XXXXXXXXXXXXX|");
      } else {
        printf("%c %11s %c",ldoor(screen),describe_screen_features(screen->features),rdoor(screen));
      }
    }
    printf("\n");
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) {
      if (screen_unreachable(screen)) {
        printf("|XXXXXXXXXXXXX|");
      } else {
        printf("%c %11s %c",ldoor(screen),ps_direction_repr(screen->direction_home),rdoor(screen));
      }
    }
    printf("\n");
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) {
      if (screen_unreachable(screen)) {
        printf("|XXXXXXXXXXXXX|");
      } else {
        printf("%c %11x %c",ldoor(screen),(int)((uintptr_t)screen->region),rdoor(screen));
      }
    }
    printf("\n");
    
    for (x=0,screen=screenrow;x<scenario->w;x++,screen++) switch (screen->doors) {
      case PS_DOOR_UNSET:  printf("O?????????????O"); break;
      case PS_DOOR_CLOSED: printf("O-------------O"); break;
      case PS_DOOR_OPEN:   printf("O             O"); break;
      default:             printf("O!!!!! %02x !!!!O",screen->doors); break;
    }
    printf("\n");
    
  }
  PS_LOG("----- end of world dump -----")
}

static void dump_scenario(struct ps_scenario *scenario) {

  if (ps_test_verbosity<0) return;

  // There used to be an object called "world", which we rolled into ps_scenario.
  dump_world(scenario);
  
}

/* Write grids to image file for review.
 */

static void write_grid_to_image(uint8_t *dst,const struct ps_grid_cell *src,int stride) {
  int y=PS_GRID_ROWC; for (;y-->0;dst+=stride,src+=PS_GRID_COLC) {
    uint8_t *subdst=dst;
    const struct ps_grid_cell *subsrc=src;
    int x=PS_GRID_COLC; for (;x-->0;subdst+=4,subsrc++) {
      switch (subsrc->physics) {
        case PS_BLUEPRINT_CELL_VACANT:   subdst[0]=0xff; subdst[1]=0xff; subdst[2]=0xff; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_SOLID:    subdst[0]=0x80; subdst[1]=0x40; subdst[2]=0x00; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_HOLE:     subdst[0]=0x00; subdst[1]=0x00; subdst[2]=0xff; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_LATCH:    subdst[0]=0x00; subdst[1]=0x80; subdst[2]=0x00; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_HEROONLY: subdst[0]=0xff; subdst[1]=0xff; subdst[2]=0x00; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_HAZARD:   subdst[0]=0xff; subdst[1]=0x00; subdst[2]=0x00; subdst[3]=0xff; break;
        case PS_BLUEPRINT_CELL_HEAL:     subdst[0]=0x00; subdst[1]=0xff; subdst[2]=0xff; subdst[3]=0xff; break;
        default:                         subdst[0]=0xff; subdst[1]=0xff; subdst[2]=0xff; subdst[3]=0xff; break;
      }
    }
  }
}

static int generate_grids_image_rgba(void *dstpp,const struct ps_scenario *scenario) {
  int w=scenario->w*PS_GRID_COLC;
  int h=scenario->h*PS_GRID_ROWC;
  int stride=w*4;
  int imgsize=stride*h;
  uint8_t *img=malloc(imgsize);
  if (!img) return -1;

  const struct ps_screen *screen=scenario->screenv;
  int screenc=scenario->w*scenario->h;
  for (;screenc-->0;screen++) {
    uint8_t *subimg=img+stride*PS_GRID_ROWC*screen->y+PS_GRID_COLC*screen->x*4;
    write_grid_to_image(subimg,screen->grid->cellv,stride);
  }

  *(void**)dstpp=img;
  return imgsize;
}

static int generate_grids_image_png(void *dstpp,const struct ps_scenario *scenario) {

  void *pixels=0;
  int pixelsc=generate_grids_image_rgba(&pixels,scenario);
  if ((pixelsc<0)||!pixels) return -1;
  
  struct akpng_image image={
    .pixels=pixels,
    .w=scenario->w*PS_GRID_COLC,
    .h=scenario->h*PS_GRID_ROWC,
    .depth=8,
    .colortype=6,
    .stride=scenario->w*PS_GRID_COLC*4,
  };
  int err=akpng_encode(dstpp,&image);
  akpng_image_cleanup(&image);
  return err;
}

static void write_grids_to_image_file(const struct ps_scenario *scenario) {

  void *src=0;
  int srcc=generate_grids_image_png(&src,scenario);
  if ((srcc<0)||!src) {
    PS_LOG("Failed to generate grid image.")
    return;
  }

  const char *path="mid/test_scgen_functional-worldmap.png";
  if (ps_file_write(path,src,srcc)<0) {
    PS_LOG("Failed to write image file '%s', %d bytes.",path,srcc)
  } else {
    PS_LOG("Wrote world map image to %s",path)
  }
  
  free(src);
}

/* Generate image of actual graphics.
 */

static struct ps_res_TILESHEET *get_tilesheet(uint8_t id) {
  struct ps_restype *restype=ps_resmgr_get_type_by_id(PS_RESTYPE_TILESHEET);
  if (!restype) return 0;
  int p=ps_restype_res_search(restype,id);
  if (p<0) return 0;
  return restype->resv[p].obj;
}

static void blackout_rgba(uint8_t *dst,int w,int h,int stride) {
  w*=4;
  while (h-->0) {
    memset(dst,0,w);
    dst+=stride;
  }
}

static void copy_rgba(uint8_t *dst,const uint8_t *src,int w,int h,int dststride,int srcstride) {
  w*=4;
  while (h-->0) {
    memcpy(dst,src,w);
    dst+=dststride;
    src+=srcstride;
  }
}

static void write_cell_graphics_to_image(uint8_t *dst,const struct ps_grid_cell *cell,int stride,struct ps_res_TILESHEET *tilesheet) {
  if (tilesheet&&tilesheet->pixels&&(tilesheet->fmt==AKGL_FMT_RGBA8)&&(tilesheet->w==16*PS_TILESIZE)&&(tilesheet->h==16*PS_TILESIZE)) {
    int srcx=(cell->tileid&0x0f)*PS_TILESIZE;
    int srcy=(cell->tileid>>4)*PS_TILESIZE;
    int srcstride=PS_TILESIZE*16*4;
    const uint8_t *src=tilesheet->pixels+srcy*srcstride+srcx*4;
    copy_rgba(dst,src,PS_TILESIZE,PS_TILESIZE,stride,srcstride);
  } else {
    blackout_rgba(dst,PS_TILESIZE,PS_TILESIZE,stride);
  }
}

static void write_grid_graphics_to_image(uint8_t *dst,const struct ps_screen *screen,const struct ps_grid_cell *src,int stride) {
  int tsid=0;
  if (screen&&screen->region) tsid=screen->region->tsid;
  struct ps_res_TILESHEET *tilesheet=get_tilesheet(tsid);
  int tilerowstride=stride*PS_TILESIZE;
  int y=PS_GRID_ROWC; for (;y-->0;dst+=tilerowstride,src+=PS_GRID_COLC) {
    uint8_t *subdst=dst;
    const struct ps_grid_cell *subsrc=src;
    int x=PS_GRID_COLC; for (;x-->0;subdst+=4*PS_TILESIZE,subsrc++) {
      write_cell_graphics_to_image(subdst,subsrc,stride,tilesheet);
    }
  }
}

static int generate_grids_graphics_image_rgba(void *dstpp,const struct ps_scenario *scenario) {
  int w=scenario->w*PS_GRID_COLC*PS_TILESIZE;
  int h=scenario->h*PS_GRID_ROWC*PS_TILESIZE;
  int stride=w*4;
  int imgsize=stride*h;
  uint8_t *img=malloc(imgsize);
  if (!img) return -1;

  const struct ps_screen *screen=scenario->screenv;
  int screenc=scenario->w*scenario->h;
  for (;screenc-->0;screen++) {
    int dstoffset=stride*PS_GRID_ROWC*PS_TILESIZE*screen->y+PS_GRID_COLC*PS_TILESIZE*screen->x*4;
    uint8_t *subimg=img+dstoffset;
    write_grid_graphics_to_image(subimg,screen,screen->grid->cellv,stride);
  }

  *(void**)dstpp=img;
  return imgsize;
}

static int generate_grids_graphics_image_png(void *dstpp,const struct ps_scenario *scenario) {

  void *pixels=0;
  int pixelsc=generate_grids_graphics_image_rgba(&pixels,scenario);
  if ((pixelsc<0)||!pixels) return -1;
  
  struct akpng_image image={
    .pixels=pixels,
    .w=scenario->w*PS_GRID_COLC*PS_TILESIZE,
    .h=scenario->h*PS_GRID_ROWC*PS_TILESIZE,
    .depth=8,
    .colortype=6,
    .stride=scenario->w*PS_GRID_COLC*PS_TILESIZE*4,
  };
  int err=akpng_encode(dstpp,&image);
  akpng_image_cleanup(&image);
  return err;
}

static void write_grids_graphics_to_image_file(const struct ps_scenario *scenario) {

  void *src=0;
  int srcc=generate_grids_graphics_image_png(&src,scenario);
  if ((srcc<0)||!src) {
    PS_LOG("Failed to generate grid graphics image.")
    return;
  }

  const char *path="mid/test_scgen_functional-graphics.png";
  if (ps_file_write(path,src,srcc)<0) {
    PS_LOG("Failed to write graphics image file '%s', %d bytes.",path,srcc)
  } else {
    PS_LOG("Wrote world map graphics image to %s",path)
  }
  
  free(src);
}

/* Dump grid graphics (for troubleshooting).
 */

static void dump_grid_graphics(struct ps_scenario *scenario,int x,int y) {
  if ((x<0)||(y<0)||(x>=scenario->w)||(y>=scenario->h)) {
    PS_LOG("Can't dump grid (%d,%d). Scenario size (%d,%d).",x,y,scenario->w,scenario->h);
    return;
  }
  struct ps_grid *grid=scenario->screenv[y*scenario->w+x].grid;
  if (!grid) return;
  PS_LOG("Dumping grid (%d,%d)...",x,y);
  const struct ps_grid_cell *cell=grid->cellv;
  int row=0; for (;row<PS_GRID_ROWC;row++) {
    int col=0; for (;col<PS_GRID_COLC;col++,cell++) {
      switch (cell->tileid) {
        case 0x45: printf(" \x1b[32mFF\x1b[0m"); break;
        case 0x46: printf(" \x1b[32mTT\x1b[0m"); break;
        case 0x47: printf(" \x1b[32m77\x1b[0m"); break;
        case 0x48: printf(" \x1b[32m|/\x1b[0m"); break;
        case 0x49: printf(" \x1b[32m\\|\x1b[0m"); break;
        case 0x55: printf(" \x1b[32mEE\x1b[0m"); break;
        case 0x56: printf(" \x1b[32mXX\x1b[0m"); break;
        case 0x57: printf(" \x1b[32m33\x1b[0m"); break;
        case 0x58: printf(" \x1b[32m|\\\x1b[0m"); break;
        case 0x59: printf(" \x1b[32m/|\x1b[0m"); break;
        case 0x65: printf(" \x1b[32mLL\x1b[0m"); break;
        case 0x66: printf(" \x1b[32mWW\x1b[0m"); break;
        case 0x67: printf(" \x1b[32mJJ\x1b[0m"); break;
        case 0x68: printf(" \x1b[32mOO\x1b[0m"); break;
        default: printf(" %02x",cell->tileid); break;
      }
    }
    printf("\n");
  }
}

/* Load resources and generate a scenario.
 */

PS_TEST(test_scgen_functional,scgen,functional,ignore) {

  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=PS_LOG_LEVEL_WARN;

  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  struct ps_scgen *scgen=ps_scgen_new();
  PS_ASSERT(scgen)

  /* Set input parameters for generator. */
  scgen->playerc=3;
  scgen->skills=PS_SKILL_HOOKSHOT|PS_SKILL_ARROW|PS_SKILL_SWORD|PS_SKILL_COMBAT;
  scgen->difficulty=5;
  scgen->length=4; // 6 seems to fit in my console pretty good.

  /* Set random seed. */
  int seed=time(0);
  //seed=1514852782;
  PS_LOG("Random seed %d",seed);
  srand(seed);

  /* Generate. */
  if (ps_scgen_generate(scgen)<0) {
    dump_scenario(scgen->scenario);
    PS_FAIL("Failed to generate scenario: %.*s",scgen->msgc,scgen->msg)
  } else {
    PS_LOG("ps_scgen_generate() OK")
    PS_ASSERT(scgen->scenario)
    dump_scenario(scgen->scenario);
    write_grids_to_image_file(scgen->scenario);
    write_grids_graphics_to_image_file(scgen->scenario);
    //dump_grid_graphics(scgen->scenario,1,3);
  }

  ps_scgen_del(scgen);
  ps_resmgr_quit();
  return 0;
}

/* Generate a scenario and analyze reuse of blueprints.
 */

static int count_blueprints_in_screen_list(const struct ps_screen *screenv,int screenc,const struct ps_blueprint *blueprint) {
  int c=0;
  for (;screenc-->0;screenv++) {
    if (screenv->blueprint==blueprint) c++;
  }
  return c;
}

static int already_visited(const int *v,int c,int q) {
  for (;c-->0;v++) if (*v==q) return 1;
  return 0;
}

static int assert_reasonable_blueprint_use(const struct ps_scenario *scenario) {
  PS_ASSERT(scenario)
  int reportedv[20];
  int reportedc=0;
  int failc=0;
  int screenc=scenario->w*scenario->h;
  const struct ps_screen *screen=scenario->screenv;
  int i=screenc; for (;i-->0;screen++) {
  
    if (screen->features&PS_SCREEN_FEATURE_TREASURE) {
      // Ignore TREASURE screens; there are only a few of them and reuse is fine.
      continue;
    }

    int blueprintid=ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,screen->blueprint);
    if (already_visited(reportedv,reportedc,blueprintid)) continue;
    int dupc=count_blueprints_in_screen_list(screen+1,i,screen->blueprint);
    if (dupc) {
      failc++;
      if (reportedc<sizeof(reportedv)/sizeof(int)) reportedv[reportedc++]=blueprintid;
      ps_log(TEST,ERROR,"blueprint:%d: %d uses",blueprintid,dupc+1);
    }

  }
  PS_ASSERT_NOT(failc,"%d non-treasure blueprint%s were reused.",failc,(failc==1)?"":"s")
  return 0;
}

PS_TEST(test_scgen_blueprint_reuse,scgen,functional,ignore) {
  ps_log_level_by_domain[PS_LOG_DOMAIN_RES]=PS_LOG_LEVEL_WARN;
  ps_resmgr_quit();
  PS_ASSERT_CALL(ps_resmgr_init("src/data",0))

  struct ps_scgen *scgen=ps_scgen_new();
  PS_ASSERT(scgen)

  /* With my current data set (5 May 2018), this configuration has 50 legal blueprints.
   * length==4 yields 30 screens.
   * So duplication, aside from TREASURE screens, should never happen.
   */
  scgen->playerc=1;
  scgen->skills=PS_SKILL_COMBAT|PS_SKILL_SWORD|PS_SKILL_HOOKSHOT;
  scgen->difficulty=9;
  scgen->length=4;

  int seed=time(0);
  PS_LOG("Random seed %d",seed);
  srand(seed);

  PS_ASSERT_CALL(ps_scgen_generate(scgen))

  if (assert_reasonable_blueprint_use(scgen->scenario)<0) return -1;

  ps_scgen_del(scgen);
  ps_resmgr_quit();
  return 0;
}
