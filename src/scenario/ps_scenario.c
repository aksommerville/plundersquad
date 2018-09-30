#include "ps.h"
#include "ps_scenario.h"
#include "ps_screen.h"
#include "ps_blueprint.h"
#include "ps_grid.h"
#include "util/ps_buffer.h"
#include "res/ps_resmgr.h"

/* New.
 */

struct ps_scenario *ps_scenario_new() {
  struct ps_scenario *scenario=calloc(1,sizeof(struct ps_scenario));
  if (!scenario) return 0;

  scenario->refc=1;

  return scenario;
}

/* Delete.
 */

void ps_scenario_del(struct ps_scenario *scenario) {
  if (!scenario) return;
  if (scenario->refc-->1) return;

  if (scenario->treasurev) free(scenario->treasurev);

  if (scenario->screenv) {
    int screenc=scenario->w*scenario->h;
    while (screenc-->0) {
      ps_screen_cleanup(scenario->screenv+screenc);
    }
    free(scenario->screenv);
  }

  free(scenario);
}

/* Retain.
 */

int ps_scenario_ref(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  if (scenario->refc<1) return -1;
  if (scenario->refc==INT_MAX) return -1;
  scenario->refc++;
  return 0;
}

/* Get screen.
 */
 
struct ps_screen *ps_scenario_get_screen(const struct ps_scenario *scenario,int x,int y) {
  if (!scenario) return 0;
  if ((x<0)||(x>=scenario->w)) return 0;
  if ((y<0)||(y>=scenario->h)) return 0;
  return PS_SCENARIO_SCREEN(scenario,x,y);
}

/* Clear.
 */

int ps_scenario_clear(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  if (ps_scenario_reallocate_screens(scenario,0,0)<0) return -1;
  scenario->homex=0;
  scenario->homey=0;
  scenario->treasurec=0;
  return 0;
}

/* Initialize screens.
 */

static void ps_scenario_seal_edges(struct ps_scenario *scenario) {
  int xz=scenario->w-1;
  int yz=scenario->h-1;
  int i=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;i-->0;screen++) {
    if (!screen->x) screen->doorw=PS_DOOR_CLOSED;
    if (!screen->y) screen->doorn=PS_DOOR_CLOSED;
    if (screen->x==xz) screen->doore=PS_DOOR_CLOSED;
    if (screen->y==yz) screen->doors=PS_DOOR_CLOSED;
  }
}

static int ps_scenario_initialize_screens(struct ps_scenario *scenario) {

  /* Set (x,y) in each screen. */
  uint8_t x=0,y=0;
  struct ps_screen *screen=scenario->screenv;;
  int screenc=scenario->w*scenario->h;
  while (screenc-->0) {
    screen->x=x;
    screen->y=y;
    if (++x>=scenario->w) {
      x=0;
      y++;
    }
    screen++;
  }

  ps_scenario_seal_edges(scenario);

  return 0;
}

/* Reallocate screens.
 */
 
int ps_scenario_reallocate_screens(struct ps_scenario *scenario,int w,int h) {
  if (!scenario) return -1;
  if ((w<0)||(h<0)||(w&&!h)||(!w&&h)) return -1;
  if (w>PS_WORLD_W_LIMIT) return -1;
  if (h>PS_WORLD_H_LIMIT) return -1;

  struct ps_screen *nv=0;
  if (w&&h) {
    nv=calloc(sizeof(struct ps_screen),w*h);
    if (!nv) return -1;
  }
  if (scenario->screenv) {
    int screenc=scenario->w*scenario->h;
    while (screenc-->0) ps_screen_cleanup(scenario->screenv+screenc);
    free(scenario->screenv);
  }
  scenario->screenv=nv;
  scenario->w=w;
  scenario->h=h;

  if (ps_scenario_initialize_screens(scenario)<0) {
    scenario->w=scenario->h=0;
    return -1;
  }
  return 0;
}

/* Reset 'visited' flags.
 */
 
int ps_scenario_reset_visited(struct ps_scenario *scenario) {
  if (!scenario) return -1;
  if (!scenario->screenv) return 0;
  int screenc=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;screenc-->0;screen++) {
    if (screen->grid) {
      screen->grid->visited=0;
    }
  }
  return 0;
}

/* Encode one screen of scenario.
 */

static int ps_scenario_encode_screen(struct ps_buffer *dst,const struct ps_screen *screen) {
  if (!screen->grid) return -1;

  int blueprintid=ps_res_get_id_by_obj(PS_RESTYPE_BLUEPRINT,screen->blueprint);
  if (blueprintid<0) {
    ps_log(RES,ERROR,"Can't encode scenario because blueprint %p not found for screen (%d,%d).",screen->blueprint,screen->x,screen->y);
    return -1;
  }
  int regionid=ps_res_get_id_by_obj(PS_RESTYPE_REGION,screen->region);
  if (regionid<0) {
    ps_log(RES,ERROR,"Can't encode scenario because region %p not found for screen (%d,%d).",screen->region,screen->x,screen->y);
    return -1;
  }

  if (ps_buffer_append_be16(dst,blueprintid)<0) return -1;
  if (ps_buffer_append_be16(dst,regionid)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->xform)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->features)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->direction_home)<0) return -1;
  if (ps_buffer_append_be8(dst,0)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->doorn)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->doors)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->doore)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->doorw)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->grid->monsterc_min)<0) return -1;
  if (ps_buffer_append_be8(dst,screen->grid->monsterc_max)<0) return -1;
  if (ps_buffer_append_be16(dst,screen->grid->poic)<0) return -1;

  const struct ps_blueprint_poi *poi=screen->grid->poiv;
  int i=screen->grid->poic;
  for (;i-->0;poi++) {
    if (ps_buffer_append_be8(dst,poi->x)<0) return -1;
    if (ps_buffer_append_be8(dst,poi->y)<0) return -1;
    if (ps_buffer_append_be8(dst,poi->type)<0) return -1;
    if (ps_buffer_append_be8(dst,0)<0) return -1;
    if (ps_buffer_append_be32(dst,poi->argv[0])<0) return -1;
    if (ps_buffer_append_be32(dst,poi->argv[1])<0) return -1;
    if (ps_buffer_append_be32(dst,poi->argv[2])<0) return -1;
  }

  /* Cells are packed in the format we want -- happily, there are no multibyte fields. */
  if (sizeof(struct ps_grid_cell)!=3) {
    ps_log(RES,ERROR,"sizeof(struct ps_grid_cell)==%d, we really thought this would be 3.",(int)sizeof(struct ps_grid_cell));
    return -1;
  }
  if (ps_buffer_append(dst,screen->grid->cellv,PS_GRID_SIZE*3)<0) return -1;
  
  return 0;
}

/* Encode scenario header.
 */

static int ps_scenario_encode_header(struct ps_buffer *dst,const struct ps_scenario *scenario) {
  uint8_t header[0x0a]={0};
  header[0x00]=scenario->w;
  header[0x01]=scenario->h;
  header[0x02]=scenario->treasurec;
  header[0x03]=scenario->homex;
  header[0x04]=scenario->homey;
  header[0x06]=PS_GRID_SIZE>>8;
  header[0x07]=PS_GRID_SIZE;
  header[0x08]=0; // extra length (MSB)
  header[0x09]=0; // extra length (LSB)
  return ps_buffer_append(dst,header,0x0a);
}

/* Encode.
 */

int ps_scenario_encode(void *dstpp,const struct ps_scenario *scenario) {
  if (!dstpp||!scenario) return -1;
  struct ps_buffer dst={0};

  if (ps_scenario_encode_header(&dst,scenario)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }

  int screenc=scenario->w*scenario->h;
  const struct ps_screen *screen=scenario->screenv;
  for (;screenc-->0;screen++) {
    if (ps_scenario_encode_screen(&dst,screen)<0) {
      ps_buffer_cleanup(&dst);
      return -1;
    }
  }

  *(void**)dstpp=dst.v;
  return dst.c;
}

/* Decode POI.
 */

static int ps_scenario_decode_poi(struct ps_blueprint_poi *poi,const uint8_t *src) {
  poi->x=src[0];
  poi->y=src[1];
  poi->type=src[2];
  poi->argv[0]=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  poi->argv[1]=(src[8]<<24)|(src[9]<<16)|(src[10]<<8)|src[11];
  poi->argv[2]=(src[12]<<24)|(src[13]<<16)|(src[14]<<8)|src[15];
  return 0;
}

/* Decode one screen.
 */

static int ps_scenario_decode_screen(struct ps_screen *screen,struct ps_scenario *scenario,const uint8_t *src,int srcc) {

  if (srcc<0x10) {
    ps_log(RES,ERROR,"Unexpected end of file decoding scenario.");
    return -1;
  }

  int blueprintid=(src[0]<<8)|src[1];
  struct ps_blueprint *blueprint=ps_res_get(PS_RESTYPE_BLUEPRINT,blueprintid);
  if (!blueprint) {
    ps_log(RES,ERROR,"blueprint:%d not found",blueprintid);
    return -1;
  }
  if (ps_screen_set_blueprint(screen,blueprint)<0) return -1;
  
  int regionid=(src[2]<<8)|src[3];
  struct ps_region *region=ps_res_get(PS_RESTYPE_REGION,regionid);
  if (!region) {
    ps_log(RES,ERROR,"region:%d not found",regionid);
    return -1;
  }
  screen->region=region;
  
  screen->xform=src[4];
  screen->features=src[5];
  screen->direction_home=src[6];
  screen->doorn=src[8];
  screen->doors=src[9];
  screen->doore=src[10];
  screen->doorw=src[11];

  if (!(screen->grid=ps_grid_new())) return -1;
  screen->grid->region=region;
  screen->grid->monsterc_min=src[12];
  screen->grid->monsterc_max=src[13];

  int poic=(src[14]<<8)|src[15];
  int srcp=16;
  if (poic) {
    if (srcp>srcc-(poic*16)) {
      ps_log(RES,ERROR,"Unexpected end of file decoding grid POI.");
      return -1;
    }
    if (!(screen->grid->poiv=calloc(poic,sizeof(struct ps_blueprint_poi)))) return -1;
    struct ps_blueprint_poi *poi=screen->grid->poiv;
    int i=0; for (;i<poic;i++,srcp+=16,poi++) {
      if (ps_scenario_decode_poi(poi,src+srcp)<0) return -1;
    }
    screen->grid->poic=poic;
  }

  if (srcp>srcc-PS_GRID_SIZE*3) {
    ps_log(RES,ERROR,"Unexpected end of file decode grid.");
    return -1;
  }
  memcpy(screen->grid->cellv,src+srcp,PS_GRID_SIZE*3);
  srcp+=PS_GRID_SIZE*3;

  return srcp;
}

/* Decode header and allocate screens.
 */

static int ps_scenario_decode_header(struct ps_scenario *scenario,const uint8_t *src,int srcc) {
  if (srcc<0x0a) {
    ps_log(RES,ERROR,"Short scenario data (%d; require at least 10).",srcc);
    return -1;
  }

  int w=src[0];
  int h=src[1];
  int treasurec=src[2];
  int homex=src[3];
  int homey=src[4];
  int gridsize=(src[6]<<8)|src[7];
  int extrac=(src[8]<<8)|src[9];

  if ((w<1)||(h<1)||(w>PS_WORLD_W_LIMIT)||(h>PS_WORLD_H_LIMIT)) {
    ps_log(RES,ERROR,"Illegal world size (%d,%d), limit (%d,%d).",w,h,PS_WORLD_W_LIMIT,PS_WORLD_H_LIMIT);
    return -1;
  }
  if ((homex>=w)||(homey>=h)) {
    ps_log(RES,ERROR,"Illegal home position (%d,%d) in world size (%d,%d).",homex,homey,w,h);
    return -1;
  }
  if (treasurec>PS_TREASURE_LIMIT) {
    ps_log(RES,ERROR,"Illegal treasure count %d (limit %d).",treasurec,PS_TREASURE_LIMIT);
    return -1;
  }
  if (gridsize!=PS_GRID_SIZE) {
    ps_log(RES,ERROR,"Serialized scenario has %d-cell grids, we only support %d (%d*%d).",gridsize,PS_GRID_SIZE,PS_GRID_COLC,PS_GRID_ROWC);
    return -1;
  }
  if (10+extrac>srcc) {
    ps_log(RES,ERROR,"Scenario header length exceeds provided data.");
    return -1;
  }
  int srcp=10+extrac;

  if (ps_scenario_reallocate_screens(scenario,w,h)<0) {
    ps_log(RES,ERROR,"Failed to allocate screens for worldl size (%d,%d).",w,h);
    return -1;
  }
  scenario->treasurec=treasurec;
  scenario->homex=homex;
  scenario->homey=homey;

  return srcp;
}

/* Decode.
 */

int ps_scenario_decode(struct ps_scenario *scenario,const void *src,int srcc) {
  if (!scenario||!src) return -1;
  if (ps_scenario_clear(scenario)<0) return -1;
  const uint8_t *SRC=src;

  int srcp=ps_scenario_decode_header(scenario,src,srcc);
  if (srcp<0) return -1;

  int screenc=scenario->w*scenario->h;
  struct ps_screen *screen=scenario->screenv;
  for (;screenc-->0;screen++) {
    int err=ps_scenario_decode_screen(screen,scenario,SRC+srcp,srcc-srcp);
    if (err<0) {
      return -1;
    }
    srcp+=err;
  }
  
  return srcp;
}
