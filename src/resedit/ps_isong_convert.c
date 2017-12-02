#include "ps.h"
#include "ps_isong.h"
#include "akau/akau.h"

/* Convert ps_isong to akau_song, main entry point.
 */
 
struct akau_song *ps_song_from_isong(const struct ps_isong *isong) {
  if (!isong) return 0;
  ps_log(AUDIO,ERROR,"TODO: %s",__func__);
  return 0;
}

/* Convert akau_song to ps_isong.
 */

struct ps_isong *ps_isong_from_song(const struct akau_song *song) {
  if (!song) return 0;
  ps_log(AUDIO,ERROR,"TODO: %s",__func__);
  return 0;
}
