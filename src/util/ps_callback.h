/* ps_callback.h
 * Generic callback structure.
 */

#ifndef PS_CALLBACK_H
#define PS_CALLBACK_H

struct ps_callback {
  int (*fn)(void *sender,void *userdata);
  void (*userdata_del)(void *userdata);
  void *userdata;
};

static inline struct ps_callback ps_callback(
  void *fn,
  void *userdata_del,
  void *userdata
) {
  return (struct ps_callback){fn,userdata_del,userdata};
}

static inline int ps_callback_call(struct ps_callback *cb,void *sender) {
  if (!cb->fn) return 0;
  return cb->fn(sender,cb->userdata);
}

static inline void ps_callback_cleanup(struct ps_callback *cb) {
  if (cb->userdata_del) {
    cb->userdata_del(cb->userdata);
    cb->userdata_del=0;
  }
}

#endif
