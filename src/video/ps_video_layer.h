/* ps_video_layer.h
 */

#ifndef PS_VIDEO_LAYER_H
#define PS_VIDEO_LAYER_H

/* Generic layer.
 *****************************************************************************/

struct ps_video_layer {
  int refc;
  int type;
  int blackout;

  void (*del)(struct ps_video_layer *layer);
  int (*draw)(struct ps_video_layer *layer);

};

struct ps_video_layer *ps_video_layer_new(int size);
void ps_video_layer_del(struct ps_video_layer *layer);
int ps_video_layer_ref(struct ps_video_layer *layer);

#endif
