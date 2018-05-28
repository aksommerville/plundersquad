/* ps_widget_levelgraph.c
 * Primitive component displaying a running graph of one channel of audio.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "sdraw/ps_sdraw.h"
#include "video/ps_video.h"
#include "akgl/akgl.h"
#include "akau/akau.h"

#define PS_LEVELGRAPH_RING_SIZE 2048

/* Object definition.
 */

struct ps_widget_levelgraph {
  struct ps_widget hdr;

  // Buffer to gather the next column:
  int samples_per_pixel;
  int16_t current_lo;
  int16_t current_hi;
  int current_samplec;

  // Buffer of columns ready for delivery to the image:
  int16_t ringv[PS_LEVELGRAPH_RING_SIZE]; // lo,hi,...,lo,hi
  int ringp;

  // Image construction.
  struct ps_sdraw_image *image;
  int ringp_drawn;

  // Texture for output.
  struct akgl_texture *texture;
  
};

#define WIDGET ((struct ps_widget_levelgraph*)widget)

/* Delete.
 */

static void _ps_levelgraph_del(struct ps_widget *widget) {
  ps_sdraw_image_del(WIDGET->image);
  akgl_texture_del(WIDGET->texture);
}

/* Initialize.
 */

static int _ps_levelgraph_init(struct ps_widget *widget) {
  if (!(WIDGET->image=ps_sdraw_image_new())) return -1;
  if (!(WIDGET->texture=akgl_texture_new())) return -1;
  return 0;
}

/* Draw.
 */

static int _ps_levelgraph_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_widget_draw_background(widget,parentx,parenty)<0) return -1;
  if (ps_video_flush_cached_drawing()<0) return -1;
  if (ps_video_draw_texture(WIDGET->texture,parentx+widget->x,parenty+widget->y,widget->w,widget->h)<0) return -1;
  return 0;
}

/* Pack.
 */

static int _ps_levelgraph_pack(struct ps_widget *widget) {
  if ((widget->w!=WIDGET->image->w)||(widget->h!=WIDGET->image->h)) {
    if (ps_sdraw_image_realloc(WIDGET->image,PS_SDRAW_FMT_A,widget->w,widget->h)<0) return -1;
  }
  WIDGET->samples_per_pixel=44100/widget->w;
  return 0;
}

/* Draw one column of image. It is previously zeroed.
 */

static int ps_levelgraph_draw_column(struct ps_widget *widget,int x,int16_t lo,int16_t hi) {
  int lop=((lo+32768)*WIDGET->image->h)/65536;
  int hip=((hi+32768)*WIDGET->image->h)/65536;
  if (ps_sdraw_draw_rect(WIDGET->image,x,lop,1,hip-lop+1,ps_sdraw_rgba(0x00,0x00,0x00,0xff))<0) return -1;
  return 0;
}

/* Scroll image and draw the new columns. Do not commit to texture yet.
 */

static int ps_levelgraph_update_image(struct ps_widget *widget) {

  /* How many columns are we adding? */
  int addc;
  if (WIDGET->ringp>WIDGET->ringp_drawn) {
    addc=WIDGET->ringp-WIDGET->ringp_drawn;
  } else {
    addc=WIDGET->ringp+(PS_LEVELGRAPH_RING_SIZE-WIDGET->ringp_drawn);
  }
  addc>>=1; // ring buffer has two items per column
  if (addc>WIDGET->image->w) addc=WIDGET->image->w;
  if (addc<1) return 0;

  /* Shuffle pixels. */
  int cpc=WIDGET->image->w-addc;
  int y=WIDGET->image->h;
  uint8_t *row=WIDGET->image->pixels;
  uint8_t *tail=row+cpc;
  for (;y-->0;row+=WIDGET->image->w,tail+=WIDGET->image->w) {
    memmove(row,row+addc,cpc);
    memset(tail,0,addc);
  }

  /* Draw new content. */
  int x=cpc;
  while (WIDGET->ringp_drawn!=WIDGET->ringp) {
    if (ps_levelgraph_draw_column(widget,x,WIDGET->ringv[WIDGET->ringp_drawn],WIDGET->ringv[WIDGET->ringp_drawn+1])<0) return -1;
    x++;
    WIDGET->ringp_drawn+=2;
    if (WIDGET->ringp_drawn>=PS_LEVELGRAPH_RING_SIZE) {
      WIDGET->ringp_drawn=0;
    }
  }
  
  return 0;
}

/* Transfer my loose image into the AKGL texture.
 */

static int ps_levelgraph_commit_to_texture(struct ps_widget *widget) {
  if (akgl_texture_load(WIDGET->texture,WIDGET->image->pixels,AKGL_FMT_A8,WIDGET->image->w,WIDGET->image->h)<0) return -1;
  return 0;
}

/* Update.
 */

static int _ps_levelgraph_update(struct ps_widget *widget) {
  int err=0;
  if (akau_lock()<0) return -1;
  if (WIDGET->ringp!=WIDGET->ringp_drawn) {
    err=ps_levelgraph_update_image(widget);
    if (!err) err=1;
  }
  if (akau_unlock()<0) return -1;
  if (err<=0) return err;
  if (ps_levelgraph_commit_to_texture(widget)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_levelgraph={

  .name="levelgraph",
  .objlen=sizeof(struct ps_widget_levelgraph),

  .del=_ps_levelgraph_del,
  .init=_ps_levelgraph_init,

  .draw=_ps_levelgraph_draw,
  .pack=_ps_levelgraph_pack,

  .update=_ps_levelgraph_update,

};

/* Add sample to buffer for pickup at next draw.
 */
 
static int ps_levelgraph_append_to_buffer(struct ps_widget *widget,int lo,int hi) {
  WIDGET->ringv[WIDGET->ringp++]=lo;
  WIDGET->ringv[WIDGET->ringp++]=hi;
  if (WIDGET->ringp>=PS_LEVELGRAPH_RING_SIZE) WIDGET->ringp=0;
  return 0;
}

/* Receive sample.
 */
 
int ps_widget_levelgraph_add_sample(struct ps_widget *widget,int16_t sample) {
  if (sample<WIDGET->current_lo) WIDGET->current_lo=sample;
  if (sample>WIDGET->current_hi) WIDGET->current_hi=sample;
  WIDGET->current_samplec++;
  if (WIDGET->current_samplec>=WIDGET->samples_per_pixel) {
    if (ps_levelgraph_append_to_buffer(widget,WIDGET->current_lo,WIDGET->current_hi)<0) return -1;
    WIDGET->current_lo=32767;
    WIDGET->current_hi=-32768;
    WIDGET->current_samplec=0;
  }
  return 0;
}
