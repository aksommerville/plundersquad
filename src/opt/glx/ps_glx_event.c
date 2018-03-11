#include "ps_glx_internal.h"

/* Key press, release, or repeat.
 */
 
static int ps_glx_evt_key(XKeyEvent *evt,int value) {
  
  /* Pass the raw keystroke. */
  KeySym keysym=XkbKeycodeToKeysym(ps_glx.dpy,evt->keycode,0,0);
  if (keysym) {
    if (ps_input_event_button(ps_glx.dev_keyboard,keysym,value)<0) {
      return -1;
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  int shift=(evt->state&ShiftMask)?1:0;
  KeySym tkeysym=XkbKeycodeToKeysym(ps_glx.dpy,evt->keycode,0,shift);
  if (tkeysym) {
    int codepoint=ps_glx_codepoint_from_keysym(tkeysym);
    if (codepoint) {
      if (ps_input_event_key(tkeysym,codepoint,value)<0) return -1;
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int ps_glx_evt_mbtn(XButtonEvent *evt,int value) {
  //ps_log(GLX,TRACE,"%s %d=%d",__func__,evt->button,value);
  switch (evt->button) {
    case 1: return ps_input_event_mbutton(1,value);
    case 2: return ps_input_event_mbutton(2,value);
    case 3: return ps_input_event_mbutton(3,value);
    case 4: if (value) return ps_input_event_mwheel(0,-1); break;
    case 5: if (value) return ps_input_event_mwheel(0,1); break;
    case 6: if (value) return ps_input_event_mwheel(-1,0); break;
    case 7: if (value) return ps_input_event_mwheel(1,0); break;
  }
  return 0;
}

static int ps_glx_evt_mmotion(XMotionEvent *evt) {
  //ps_log(GLX,TRACE,"%s %d,%d",__func__,evt->x,evt->y);
  if (ps_input_event_mmotion(evt->x,evt->y)<0) return -1;
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
    
    case ButtonPress: return ps_glx_evt_mbtn(&evt->xbutton,1);
    case ButtonRelease: return ps_glx_evt_mbtn(&evt->xbutton,0);
    case MotionNotify: return ps_glx_evt_mmotion(&evt->xmotion);
    
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
