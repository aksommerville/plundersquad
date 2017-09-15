#include "ps_video_internal.h"

/* New.
 */

struct ps_video_layer *ps_video_layer_new(int size) {
  if (size<(int)sizeof(struct ps_video_layer)) return 0;

  struct ps_video_layer *layer=calloc(1,size);
  if (!layer) return 0;

  layer->refc=1;

  return layer;
}

/* Delete.
 */

void ps_video_layer_del(struct ps_video_layer *layer) {
  if (!layer) return;
  if (layer->refc-->1) return;
  
  if (layer->del) layer->del(layer);

  free(layer);
}

/* Retain.
 */

int ps_video_layer_ref(struct ps_video_layer *layer) {
  if (!layer) return -1;
  if (layer->refc<1) return -1;
  if (layer->refc==INT_MAX) return -1;
  layer->refc++;
  return 0;
}
