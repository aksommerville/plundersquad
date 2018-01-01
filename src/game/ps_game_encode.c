#include "ps.h"
#include "ps_game.h"
#include "ps_stats.h"
#include "ps_player.h"
#include "ps_plrdef.h"
#include "scenario/ps_scenario.h"
#include "res/ps_resmgr.h"
#include "util/ps_buffer.h"
#include <zlib.h>

/* Encode header.
 */

static int ps_game_encode_header(struct ps_buffer *dst,const struct ps_game *game) {
 
  uint8_t tmp[30]={0};
  memcpy(tmp,"\0PLSQD\n\xff",8);
  tmp[11]=1; // Serial version
  tmp[16]=game->playerc;
  tmp[17]=game->difficulty;
  tmp[18]=game->length;
  uint32_t treasurebits=0;
  int i=game->treasurec; for (;i-->0;) if (game->treasurev[i]) treasurebits|=1<<i;
  tmp[20]=treasurebits>>24;
  tmp[21]=treasurebits>>16;
  tmp[22]=treasurebits>>8;
  tmp[23]=treasurebits;
  tmp[24]=game->stats->playtime>>24;
  tmp[25]=game->stats->playtime>>16;
  tmp[26]=game->stats->playtime>>8;
  tmp[27]=game->stats->playtime;
  tmp[28]=game->gridx;
  tmp[29]=game->gridy;

  if (ps_buffer_append(dst,tmp,30)<0) return -1;
  
  return 0;
}

/* Encode players.
 */

static int ps_game_encode_players(struct ps_buffer *buffer,const struct ps_game *game) {
  int i=0; for (;i<game->playerc;i++) {
    struct ps_player *player=game->playerv[i];
    int plrdefid=ps_res_get_id_by_obj(PS_RESTYPE_PLRDEF,player->plrdef);
    if (plrdefid<0) {
      ps_log(RES,ERROR,"Can't encode game: plrdef not found for %p",player->plrdef);
      return -1;
    }
    uint8_t tmp[4]={0};
    tmp[0]=plrdefid>>8;
    tmp[1]=plrdefid;
    tmp[2]=player->palette;
    if (ps_buffer_append(buffer,tmp,4)<0) return -1;
  }
  return 0;
}

/* Encode scenario.
 */

static int ps_game_encode_scenario(struct ps_buffer *buffer,const struct ps_game *game) {

  int preamblep=buffer->c;
  if (ps_buffer_append(buffer,"        ",8)<0) return -1; // Dummy preamble, we will fill in after encoding and compressing.

  void *serial=0;
  int serialc=ps_scenario_encode(&serial,game->scenario);
  if ((serialc<0)||!serial) {
    ps_log(RES,ERROR,"Failed to encode scenario.");
    return -1;
  }

  int serialstartp=buffer->c;
  if (ps_buffer_compress_and_append(buffer,serial,serialc)<0) {
    free(serial);
    return -1;
  }
  free(serial);
  
  int clen=buffer->c-serialstartp;
  uint8_t *preamble=(uint8_t*)buffer->v+preamblep;
  preamble[0]=clen>>24;
  preamble[1]=clen>>16;
  preamble[2]=clen>>8;
  preamble[3]=clen;
  preamble[4]=serialc>>24;
  preamble[5]=serialc>>16;
  preamble[6]=serialc>>8;
  preamble[7]=serialc;

  return 0;
}

/* Encode game.
 */
 
int ps_game_encode(void *dstpp,const struct ps_game *game) {
  if (!dstpp||!game) return -1;
  struct ps_buffer dst={0};

  if (ps_game_encode_header(&dst,game)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }

  if (ps_game_encode_players(&dst,game)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }

  if (ps_game_encode_scenario(&dst,game)<0) {
    ps_buffer_cleanup(&dst);
    return -1;
  }

  *(void**)dstpp=dst.v;
  return dst.c;
}

/* Decode game header.
 */
 
static int ps_game_decode_header(struct ps_game *game,const uint8_t *src,int srcc) {

  if (srcc<30) {
    ps_log(RES,ERROR,"Failed to decode game: short data");
    return -1;
  }

  if (memcmp(src,"\0PLSQD\n\xff",8)) {
    ps_log(RES,ERROR,"Failed to decode game: signature mismatch");
    return -1;
  }

  int gameversion=(src[8]<<24)|(src[9]<<16)|(src[10]<<8)|src[11];
  int resversion=(src[12]<<24)|(src[13]<<16)|(src[14]<<8)|src[15];
  //TODO validate game and resource versions.

  int playerc=src[16];
  int difficulty=src[17];
  int length=src[18];
  if ((playerc<1)||(playerc>PS_PLAYER_LIMIT)) {
    ps_log(RES,ERROR,"Illegal player count in serialized game: %d",playerc);
    return -1;
  }
  if ((difficulty<PS_DIFFICULTY_MIN)||(difficulty>PS_DIFFICULTY_MAX)) {
    ps_log(RES,ERROR,"Illegal difficulty in serialized game: %d",difficulty);
    return -1;
  }
  if ((length<PS_LENGTH_MIN)||(length>PS_LENGTH_MAX)) {
    ps_log(RES,ERROR,"Illegal length in serialized game: %d",length);
    return -1;
  }
  
  if (ps_game_set_player_count(game,playerc)<0) return -1;
  
  game->difficulty=difficulty;
  game->length=length;

  uint32_t treasurebits=(src[20]<<24)|(src[21]<<16)|(src[22]<<8)|src[23];
  uint32_t mask=1;
  int i=0; for (;i<32;i++,mask<<=1) {
    game->treasurev[i]=(treasurebits&mask)?1:0;
  }

  game->stats->playtime=(src[24]<<24)|(src[25]<<16)|(src[26]<<8)|src[27];
  game->gridx=src[28];
  game->gridy=src[29];

  return 30;
}

/* Decode players.
 */

static int ps_game_decode_players(struct ps_game *game,const uint8_t *src,int srcc) {
  int srcp=0;
  int reqc=4*game->playerc;
  if (srcc<reqc) {
    ps_log(RES,ERROR,"Short data reading players from serialized game.");
    return -1;
  }

  int i=0; for (;i<game->playerc;i++,srcp+=4) {
    struct ps_player *player=game->playerv[i];
    int plrdefid=(src[srcp]<<8)|src[srcp+1];
    struct ps_plrdef *plrdef=ps_res_get(PS_RESTYPE_PLRDEF,plrdefid);
    if (!plrdef) {
      ps_log(RES,ERROR,"plrdef:%d not found",plrdefid);
      return -1;
    }
    player->plrdef=plrdef;
    player->palette=src[srcp+2];
    if (player->palette>=plrdef->palettec) {
      // Invalid palette, whatever.
      player->palette=0;
    }
  }

  return srcp;
}

/* Decode scenario.
 */

static int ps_game_decode_scenario_inner(struct ps_game *game,const void *src,int srcc) {
  ps_scenario_del(game->scenario);
  if (!(game->scenario=ps_scenario_new())) return -1;
  if (ps_scenario_decode(game->scenario,src,srcc)<0) return -1;
  game->treasurec=game->scenario->treasurec;
  return 0;
}

/* Decode scenario, framing and decompression.
 */

static int ps_game_decode_scenario(struct ps_game *game,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp>srcc-8) {
    ps_log(RES,ERROR,"Short data reading scenario from serialized game.");
    return -1;
  }
  int clen=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int dlen=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  srcp+=8;
  if (srcp>srcc-clen) {
    ps_log(RES,ERROR,"Short data reading scenario from serialized game.");
    return -1;
  }
  if ((clen<0)||(dlen<0)) {
    ps_log(RES,ERROR,"Invalid scenario length in serialized game.");
    return -1;
  }

  void *ddata=malloc(dlen);
  if (!ddata) return -1;
  uLongf destLen=dlen;
  int err=uncompress((Bytef*)ddata,&destLen,(Bytef*)(src+srcp),clen);
  if (err!=Z_OK) {
    ps_log(RES,ERROR,"Error %d decompressing scenario.",err);
    return -1;
  }

  if (ps_game_decode_scenario_inner(game,ddata,destLen)<0) {
    free(ddata);
    return -1;
  }
  free(ddata);

  srcp+=clen;
  return srcp;
}

/* Decode game.
 */
 
int ps_game_decode(struct ps_game *game,const void *src,int srcc) {
  if (!game||!src) return -1;
  const uint8_t *SRC=src;
  int srcp=0,err;

  if ((err=ps_game_decode_header(game,SRC+srcp,srcc-srcp))<0) return -1;
  srcp+=err;

  if ((err=ps_game_decode_players(game,SRC+srcp,srcc-srcp))<0) return -1;
  srcp+=err;

  if ((err=ps_game_decode_scenario(game,SRC+srcp,srcc-srcp))<0) return -1;
  srcp+=err;

  if ((game->gridx>=game->scenario->w)||(game->gridy>=game->scenario->h)) {
    ps_log(RES,ERROR,"Current screen (%d,%d) out of bounds (%d,%d).",game->gridx,game->gridy,game->scenario->w,game->scenario->h);
    return -1;
  }
  
  return srcp;
}
