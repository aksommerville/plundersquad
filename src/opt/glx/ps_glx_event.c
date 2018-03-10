#include "ps_glx_internal.h"

/* Key press, release, or repeat.
 */
 
static int ps_glx_evt_key(XKeyEvent *evt,int value) {
  int shift=(evt->state&ShiftMask)?1:0;
  shift=0;//TODO Dropping key shift -- we would need this if we are receiving proper text.
  KeySym keysym=XkbKeycodeToKeysym(ps_glx.dpy,evt->keycode,0,shift);
  if (keysym) {
    //TODO text from keyboard event
    if (ps_input_event_button(ps_glx.dev_keyboard,keysym,value)<0) {
      return -1;
    }
  }
  return 0;
}

/* Client message.
 */
 
static int ps_glx_evt_client(XClientMessageEvent *evt) {
  //mpi_log(EVENT,"X11 ClientMessage");
  //mpi_log(DEBUG,"message_type %s",XGetAtomName(ps_glx.dpy,evt->message_type));//XXX memory leak
  if (evt->message_type==ps_glx.atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==ps_glx.atom_WM_DELETE_WINDOW) {
        ps_log(GLX,TRACE,"WM_DELETE_WINDOW");
        if (ps_input_request_termination()<0) {
          return -1;
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int ps_glx_evt_configure(XConfigureEvent *evt) {
  //mpi_log(EVENT,"X11 ConfigureNotify (%d,%d,%d,%d)",evt->x,evt->y,evt->width,evt->height);
  int nw=evt->width,nh=evt->height;
  if ((nw!=ps_glx.w)||(nh!=ps_glx.h)) {
    ps_glx.w=nw;
    ps_glx.h=nh;
    ps_log(GLX,TRACE,"resize %d,%d",nw,nh);
    if (ps_video_resized(nw,nh)<0) {
      return -1;
    }
  }
  return 0;
}

/* Dispatch single event.
 */
 
static int ps_glx_receive_event(XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return ps_glx_evt_key(&evt->xkey,1);
    case KeyRelease: return ps_glx_evt_key(&evt->xkey,0);
    case KeyRepeat: return ps_glx_evt_key(&evt->xkey,2);
    
    case ClientMessage: return ps_glx_evt_client(&evt->xclient);
    
    case ConfigureNotify: return ps_glx_evt_configure(&evt->xconfigure);
    
    default: {
        ps_log(GLX,TRACE,"X11 event type %d.",evt->type);
      }
  }
  return 0;
}

/* Update.
 */
 
int ps_glx_update(struct ps_input_provider *provider) {
  if (!ps_glx.dpy) return 0;
  int evtc=XEventsQueued(ps_glx.dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(ps_glx.dpy,&evt);
    
    /* If we detect an auto-repeated key, drop one of the events, and turn the other into KeyRepeat.
     * This is a hack to force single events for key repeat.
     */
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(ps_glx.dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-PS_GLX_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (ps_glx_receive_event(&evt)<0) return -1;
      } else {
        if (ps_glx_receive_event(&evt)<0) return -1;
        if (ps_glx_receive_event(&next)<0) return -1;
      }
    } else {
      if (ps_glx_receive_event(&evt)<0) return -1;
    }
  }
  return 0;
}
