#include "ps.h"
#include "ps_sem.h"
#include "akau/akau.h"

/* Object lifecycle.
 */

struct ps_sem *ps_sem_new() {
  struct ps_sem *sem=calloc(1,sizeof(struct ps_sem));
  if (!sem) return 0;

  sem->refc=1;
  sem->noteid_next=1;

  return sem;
}

static void ps_sem_beat_cleanup(struct ps_sem_beat *beat) {
  if (!beat) return;
  if (beat->eventv) free(beat->eventv);
}

void ps_sem_del(struct ps_sem *sem) {
  if (!sem) return;
  if (sem->refc-->1) return;

  if (sem->beatv) {
    while (sem->beatc-->0) {
      ps_sem_beat_cleanup(sem->beatv+sem->beatc);
    }
    free(sem->beatv);
  }

  free(sem);
}

int ps_sem_ref(struct ps_sem *sem) {
  if (!sem) return -1;
  if (sem->refc<1) return -1;
  if (sem->refc==INT_MAX) return -1;
  sem->refc++;
  return 0;
}

/* "start" vs "end" operations.
 */

static int ps_sem_op_is_start(uint8_t op) {
  switch (op) {
    case AKAU_SONG_OP_NOTE:
    case AKAU_SONG_OP_ADJPITCH:
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN:
      return 1;
  }
  return 0;
}

static int ps_sem_op_is_end(uint8_t op) {
  switch (op) {
    case PS_SEM_OP_NOTEEND:
    case PS_SEM_OP_ADJPITCHEND:
    case PS_SEM_OP_ADJTRIMEND:
    case PS_SEM_OP_ADJPANEND:
      return 1;
  }
  return 0;
}

static uint8_t ps_sem_op_partner(uint8_t op) {
  switch (op) {
    case AKAU_SONG_OP_NOTE: return PS_SEM_OP_NOTEEND;
    case AKAU_SONG_OP_ADJPITCH: return PS_SEM_OP_ADJPITCHEND;
    case AKAU_SONG_OP_ADJTRIM: return PS_SEM_OP_ADJTRIMEND;
    case AKAU_SONG_OP_ADJPAN: return PS_SEM_OP_ADJPANEND;
    case PS_SEM_OP_NOTEEND: return AKAU_SONG_OP_NOTE;
    case PS_SEM_OP_ADJPITCHEND: return AKAU_SONG_OP_ADJPITCH;
    case PS_SEM_OP_ADJTRIMEND: return AKAU_SONG_OP_ADJTRIM;
    case PS_SEM_OP_ADJPANEND: return AKAU_SONG_OP_ADJPAN;
  }
  return op;
}

/* After removing beats, read the entire event list and add or remove events to force integrity.
 * - Insert missing sentinels at (rmp) if sensible*, or on the same beat as the starting event.
 * - Remove sentinels whose partner is missing.
 * * I'm not actually doing this yet, just insert on the same beat.
 */

static int ps_sem_sanitize_sentinels_after_beat_removal(struct ps_sem *sem,int rmp) {
  int beatp=0; for (;beatp<sem->beatc;beatp++) {
    struct ps_sem_beat *beat=sem->beatv+beatp;
    int eventp=0; while (eventp<beat->eventc) {
      struct ps_sem_event *event=beat->eventv+eventp;
      int partnerbeatp,partnereventp;
      if (ps_sem_find_partner(&partnerbeatp,&partnereventp,sem,beatp,eventp)<0) {
        if (ps_sem_op_is_end(event->op)) {
        
          // Sentinel event, partner missing: remove event.
          beat->eventc--;
          memmove(event,event+1,sizeof(struct ps_sem_event)*(beat->eventc-eventp));
          
        } else {

          // Starting event, partner missing: insert sentinel.
          struct ps_sem_event evcopy=*event; // Creating the new one invalidates (event).
          struct ps_sem_event *end=ps_sem_beat_new_event(beat);
          if (!end) return -1;
          memcpy(end,&evcopy,sizeof(struct ps_sem_event));
          end->op=ps_sem_op_partner(evcopy.op);
          eventp++;
          
        }
      } else {
      
        // Found the partner, all good.
        eventp++;
        
      }
    }
  }
  return 0;
}

/* Beat list.
 */

static int ps_sem_beatv_require(struct ps_sem *sem,int addc) {
  if (addc<1) return 0;
  if (sem->beatc>INT_MAX-addc) return -1;
  int na=sem->beatc+addc;
  if (na<=sem->beata) return 0;
  if (na<INT_MAX-256) na=(na+256)&~255;
  if (na>INT_MAX/sizeof(struct ps_sem_beat)) return -1;
  void *nv=realloc(sem->beatv,sizeof(struct ps_sem_beat)*na);
  if (!nv) return -1;
  sem->beatv=nv;
  sem->beata=na;
  return 0;
}
 
int ps_sem_remove_beats(struct ps_sem *sem,int p,int c) {
  if (!sem) return -1;
  if (c<0) {
    if ((p<0)||(p>=sem->beatc)) return 0;
    c=sem->beatc-p;
  }
  if (p<0) {
    if (c<1) return 0;
    if (c>sem->beatc) c=sem->beatc;
    p=sem->beatc-c;
  }
  if (p>sem->beatc-c) c=sem->beatc-p;
  if (c<1) return 0;
  int i=c; while (i-->0) ps_sem_beat_cleanup(sem->beatv+p+i);
  sem->beatc-=c;
  memmove(sem->beatv+p,sem->beatv+p+c,sizeof(struct ps_sem_beat)*(sem->beatc-p));
  if (ps_sem_sanitize_sentinels_after_beat_removal(sem,p)<0) return -1;
  return 0;
}

int ps_sem_insert_beats(struct ps_sem *sem,int p,int c) {
  if (!sem) return -1;
  if (c<1) return 0;
  if ((p<0)||(p>sem->beatc)) p=sem->beatc;
  if (ps_sem_beatv_require(sem,c)<0) return -1;
  memmove(sem->beatv+p+c,sem->beatv+p,sizeof(struct ps_sem_beat)*(sem->beatc-p));
  memset(sem->beatv+p,0,sizeof(struct ps_sem_beat)*c);
  sem->beatc+=c;
  return 0;
}

int ps_sem_set_beat_count(struct ps_sem *sem,int beatc) {
  if (!sem) return -1;
  if (beatc<sem->beatc) return ps_sem_remove_beats(sem,beatc,sem->beatc-beatc);
  else if (beatc>sem->beatc) return ps_sem_insert_beats(sem,sem->beatc,beatc-sem->beatc);
  return 0;
}

/* Add blank event to beat.
 */
 
struct ps_sem_event *ps_sem_beat_new_event(struct ps_sem_beat *beat) {
  if (!beat) return 0;
  if (beat->eventc>=beat->eventa) {
    int na=beat->eventa+4;
    if (na>INT_MAX/sizeof(struct ps_sem_event)) return 0;
    void *nv=realloc(beat->eventv,sizeof(struct ps_sem_event)*na);
    if (!nv) return 0;
    beat->eventv=nv;
    beat->eventa=na;
  }
  struct ps_sem_event *event=beat->eventv+beat->eventc++;
  memset(event,0,sizeof(struct ps_sem_event));
  return event;
}

/* Remove events from beat.
 */
 
int ps_sem_beat_remove_events(struct ps_sem_beat *beat,int p,int c) {
  if (!beat) return -1;
  if (p<0) return -1;
  if (c<1) return 0;
  if (p>beat->eventc-c) return -1;
  beat->eventc-=c;
  memmove(beat->eventv+p,beat->eventv+p+c,sizeof(struct ps_sem_event)*(beat->eventc-p));
  return 0;
}

/* Add events, user-friendly.
 */
 
int ps_sem_add_note(struct ps_sem *sem,int beatp,uint8_t instrid,uint8_t pitch,uint8_t trim,int8_t pan) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  
  int noteid=sem->noteid_next++;
  struct ps_sem_event *ea=ps_sem_beat_new_event(beat);
  if (!ea) return -1;
  struct ps_sem_event *ez=ps_sem_beat_new_event(beat);
  if (!ez) return -1;
  ea=ez-1; // Creating (ez) might have reallocated the event list.

  ea->op=AKAU_SONG_OP_NOTE;
  ez->op=PS_SEM_OP_NOTEEND;
  ea->noteid=ez->noteid=noteid;
  ea->objid=ez->objid=instrid;
  ea->pitch=ez->pitch=pitch;
  ea->trim=ez->trim=trim;
  ea->pan=ez->pan=pan;

  return noteid;
}

int ps_sem_add_drum(struct ps_sem *sem,int beatp,uint8_t drumid,uint8_t trim,int8_t pan) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  
  struct ps_sem_event *event=ps_sem_beat_new_event(beat);
  if (!event) return -1;

  event->op=AKAU_SONG_OP_DRUM;
  event->objid=drumid;
  event->trim=trim;
  event->pan=pan;

  return 0;
}

/* Change pitch.
 */

static int ps_sem_adjust_chain_pitch(struct ps_sem *sem,int beatp,int noteid,int pitch,int beatd) {

  /* When (beatd<0) and we find ADJPITCH, modify only the END events, and terminate.
   * When (beatd>0) and we find ADJPITCHEND, modify only the START events, and terminate.
   * Otherwise, modify everything on this beat and advance to the next one.
   * But on top of all that: Don't stop at the initial event.
   */

  while ((beatp>=0)&&(beatp<sem->beatc)) {
    struct ps_sem_beat *beat=sem->beatv+beatp;

    // Take a good look at this beat.
    struct ps_sem_event *ievent=beat->eventv;
    int ieventp=0;
    int have_ADJPITCH=0,have_ADJPITCHEND=0,have_NOTE=0,have_NOTEEND=0;
    for (;ieventp<beat->eventc;ieventp++,ievent++) {
      if (ievent->noteid==noteid) {
        switch (ievent->op) {
          case AKAU_SONG_OP_NOTE: have_NOTE=1; break;
          case AKAU_SONG_OP_ADJPITCH: have_ADJPITCH=1; break;
          case PS_SEM_OP_NOTEEND: have_NOTEEND=1; break;
          case PS_SEM_OP_ADJPITCHEND: have_ADJPITCHEND=1; break;
        }
      }
    }

    // Modify events selectively and terminate if we found a pitch adjustment.
    if ((beatd<0)&&have_ADJPITCH) {
      for (ieventp=0,ievent=beat->eventv;ieventp<beat->eventc;ieventp++,ievent++) {
        if (ievent->noteid==noteid) {
          switch (ievent->op) {
            case PS_SEM_OP_NOTEEND:
            case PS_SEM_OP_ADJPITCHEND:
            case PS_SEM_OP_ADJTRIMEND:
            case PS_SEM_OP_ADJPANEND: {
                ievent->pitch=pitch;
              } break;
          }
        }
      }
      return 0;
    }
    if ((beatd>0)&&have_ADJPITCHEND) {
      for (ieventp=0,ievent=beat->eventv;ieventp<beat->eventc;ieventp++,ievent++) {
        if (ievent->noteid==noteid) {
          switch (ievent->op) {
            case AKAU_SONG_OP_NOTE:
            case AKAU_SONG_OP_ADJPITCH:
            case AKAU_SONG_OP_ADJTRIM:
            case AKAU_SONG_OP_ADJPAN: {
                ievent->pitch=pitch;
              } break;
          }
        }
      }
      return 0;
    }

    // Modify everything.
    for (ieventp=0,ievent=beat->eventv;ieventp<beat->eventc;ieventp++,ievent++) {
      if (ievent->noteid==noteid) {
        ievent->pitch=pitch;
      }
    }

    // If we found the note's start or end, we can terminate.
    if ((beatd<0)&&have_NOTE) return 0;
    if ((beatd>0)&&have_NOTEEND) return 0;

    beatp+=beatd;
  }
  return 0;
}
 
int ps_sem_adjust_event_pitch(struct ps_sem *sem,int beatp,int eventp,int pitch) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return -1;
  struct ps_sem_event *event=beat->eventv+eventp;
  if (pitch<0) pitch=0;
  else if (pitch>255) pitch=255;

  switch (event->op) {

    case AKAU_SONG_OP_NOTE:
    case AKAU_SONG_OP_ADJPITCH:
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN: {
        if (ps_sem_adjust_chain_pitch(sem,beatp-1,event->noteid,pitch,-1)<0) return -1;
        if (ps_sem_adjust_chain_pitch(sem,beatp,event->noteid,pitch,1)<0) return -1;
      } break;
    
    case PS_SEM_OP_NOTEEND:
    case PS_SEM_OP_ADJPITCHEND:
    case PS_SEM_OP_ADJTRIMEND:
    case PS_SEM_OP_ADJPANEND: {
        if (ps_sem_adjust_chain_pitch(sem,beatp,event->noteid,pitch,-1)<0) return -1;
        if (ps_sem_adjust_chain_pitch(sem,beatp+1,event->noteid,pitch,1)<0) return -1;
      } break;

  }
  return 0;
}

/* Change properties across an entire note.
 */
 
int ps_sem_modify_note(struct ps_sem *sem,int beatp,int eventp,int objid,int trim,int pan) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return -1;
  struct ps_sem_event *event=beat->eventv+eventp;

  if (event->noteid) {
    int noteid=event->noteid;
    if (ps_sem_find_note_start(&beatp,&eventp,sem,beatp,eventp)<0) return -1;
    while (1) {
      event=sem->beatv[beatp].eventv+eventp;
      event->objid=objid;
      event->trim=trim;
      event->pan=pan;
      if (ps_sem_find_note_next(&beatp,&eventp,sem,beatp,eventp)<0) break;
    }
  } else {
    event->objid=objid;
    event->trim=trim;
    event->pan=pan;
  }

  return 0;
}

/* Remove note from SEM by noteid.
 */
 
int ps_sem_remove_note(struct ps_sem *sem,int noteid) {
  if (!sem) return -1;
  if (noteid<1) return -1;
  struct ps_sem_beat *beat=sem->beatv;
  int beatp=0; for (;beatp<sem->beatc;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;) {
      if (event->noteid==noteid) {
        int done=0;
        if (event->op==PS_SEM_OP_NOTEEND) done=1;
        beat->eventc--;
        memmove(event,event+1,sizeof(struct ps_sem_event)*(beat->eventc-eventp));
        if (done) return 0;
      } else {
        eventp++;
        event++;
      }
    }
  }
  return -1;
}

/* Remove event and dependents.
 */
 
int ps_sem_remove_event(struct ps_sem *sem,int beatp,int eventp) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  if ((eventp<0)||(eventp>=sem->beatv[beatp].eventc)) return -1;

  const struct ps_sem_event *event=sem->beatv[beatp].eventv+eventp;
  switch (event->op) {

    case AKAU_SONG_OP_NOTE: return ps_sem_remove_note(sem,event->noteid);
    case PS_SEM_OP_NOTEEND: return ps_sem_remove_note(sem,event->noteid);

    case AKAU_SONG_OP_ADJPITCH:
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN:
    case PS_SEM_OP_ADJPITCHEND:
    case PS_SEM_OP_ADJTRIMEND:
    case PS_SEM_OP_ADJPANEND: {
        int partnerbeatp,partnereventp;
        if (ps_sem_find_partner(&partnerbeatp,&partnereventp,sem,beatp,eventp)<0) break;
        if (partnereventp>eventp) { // They might be on the same beat, so be sure to remove the higher eventp first.
          if (ps_sem_beat_remove_events(sem->beatv+partnerbeatp,partnereventp,1)<0) return -1;
          if (ps_sem_beat_remove_events(sem->beatv+beatp,eventp,1)<0) return -1;
        } else {
          if (ps_sem_beat_remove_events(sem->beatv+beatp,eventp,1)<0) return -1;
          if (ps_sem_beat_remove_events(sem->beatv+partnerbeatp,partnereventp,1)<0) return -1;
        }
      } return 0;

  }
  return ps_sem_beat_remove_events(sem->beatv+beatp,eventp,1);
}

/* Move event.
 */
 
int ps_sem_move_event(struct ps_sem *sem,int beatp,int eventp,int dstbeatp) {
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return -1;
  if ((dstbeatp<0)||(dstbeatp>=sem->beatc)) return -1;
  if (beatp==dstbeatp) return eventp;
  struct ps_sem_beat *nbeat=sem->beatv+dstbeatp;

  /* Assert continuity. */
  int beatplo,beatphi;
  if (ps_sem_get_event_movement_bounds(&beatplo,&beatphi,sem,beatp,eventp)<0) return -1;
  if (dstbeatp<beatplo) return -1;
  if (dstbeatp>beatphi) return -1;

  /* Move it. */
  struct ps_sem_event *nevent=ps_sem_beat_new_event(nbeat);
  if (!nevent) return -1;
  memcpy(nevent,beat->eventv+eventp,sizeof(struct ps_sem_event));
  if (ps_sem_beat_remove_events(beat,eventp,1)<0) return -1;
  int neventp=nevent-nbeat->eventv;
  
  return neventp;
}

/* Set note length by noteid.
 */
 
int ps_sem_set_note_length(struct ps_sem *sem,int noteid,int beatc) {
  if (!sem) return -1;
  if (beatc<1) return -1;
  if (noteid<1) return -1;
  int startbeatp,starteventp,endbeatp,endeventp;
  if (ps_sem_find_note_by_noteid(&startbeatp,&starteventp,sem,noteid)<0) return -1;
  if (ps_sem_find_partner(&endbeatp,&endeventp,sem,startbeatp,starteventp)<0) return -1;
  if (ps_sem_move_event(sem,endbeatp,endeventp,startbeatp+beatc-1)<0) return -1;
  return 0;
}

/* Search for note by noteid.
 */
 
int ps_sem_find_note_by_noteid(int *beatp,int *eventp,const struct ps_sem *sem,int noteid) {
  if (!beatp||!eventp||!sem) return -1;
  if (noteid<1) return -1;
  const struct ps_sem_beat *beat=sem->beatv;
  int qbeatp=0; for (;qbeatp<sem->beatc;qbeatp++,beat++) {
    const struct ps_sem_event *event=beat->eventv;
    int qeventp=0; for (;qeventp<beat->eventc;qeventp++,event++) {
      if (event->op!=AKAU_SONG_OP_NOTE) continue;
      if (event->noteid!=noteid) continue;
      *beatp=qbeatp;
      *eventp=qeventp;
      return 0;
    }
  }
  return -1;
}

/* Measure movement boundaries.
 */
 
int ps_sem_get_event_movement_bounds(int *beatplo,int *beatphi,const struct ps_sem *sem,int beatp,int eventp) {
  if (!beatplo||!beatphi||!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  if ((eventp<0)||(eventp>=sem->beatv[beatp].eventc)) return -1;
  *beatplo=0;
  *beatphi=sem->beatc-1;

  /* If it doesn't have a noteid, it can move anywhere. */
  int noteid=sem->beatv[beatp].eventv[eventp].noteid;
  if (!noteid) return 0;
  
  uint8_t op=sem->beatv[beatp].eventv[eventp].op;
  uint8_t opposite;
  switch (op) {

      /* NOTE:
       *  - Moving down is free.
       *  - Anything for this noteid, except this command, *before* the destination beat, is an error.
       */
      case AKAU_SONG_OP_NOTE: {
          int i=beatp; for (;i<sem->beatc;i++) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if ((i==beatp)&&(ei==eventp)) continue;
                *beatphi=i; // We can move here but no further.
                return 0;
              }
            }
          }
        } break;

      /* NOTEEND:
       *  - Moving up is free.
       *  - Moving down, we must not cross anything on this noteid (except ourself).
       */
      case PS_SEM_OP_NOTEEND: {
          int i=beatp; for (;i>=0;i--) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if ((i==beatp)&&(ei==eventp)) continue;
                *beatplo=i; // We can move here but no further
                return 0;
              }
            }
          }
        } break;

      /* ADJ:
       *  - Moving down, we must not meet the opposite event, and we must not cross the NOTE.
       *  - Moving up, we must not cross the opposite event or the NOTEEND.
       */
      case AKAU_SONG_OP_ADJPITCH: opposite=PS_SEM_OP_ADJPITCHEND; goto _adj_;
      case AKAU_SONG_OP_ADJTRIM: opposite=PS_SEM_OP_ADJTRIMEND; goto _adj_;
      case AKAU_SONG_OP_ADJPAN: opposite=PS_SEM_OP_ADJPANEND; goto _adj_;
      _adj_: {
          int i;
          for (i=beatp;i<sem->beatc;i++) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if (event->op==opposite) {
                  *beatphi=i;
                  goto _done_adj_up_;
                }
              }
            }
          }
         _done_adj_up_:;
          for (i=beatp;i>=0;i--) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int stop_here=0;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if ((event->op==opposite)&&(i<beatp)) {
                  *beatplo=i+1;
                  goto _done_adj_down_;
                }
                if (event->op==AKAU_SONG_OP_NOTE) {
                  stop_here=1;
                }
              }
            }
            if (stop_here) {
              *beatplo=i;
              goto _done_adj_down_;
            }
          }
         _done_adj_down_:;
        } break;

      /* ADJEND:
       *  - Moving down, we must not cross the opposite event.
       *  - Moving up, we must not meet the opposite event, and must not cross NOTEEND.
       */
      case PS_SEM_OP_ADJPITCHEND: opposite=AKAU_SONG_OP_ADJPITCH; goto _adjend_;
      case PS_SEM_OP_ADJTRIMEND: opposite=AKAU_SONG_OP_ADJTRIM; goto _adjend_;
      case PS_SEM_OP_ADJPANEND: opposite=AKAU_SONG_OP_ADJPAN; goto _adjend_;
      _adjend_: {
          int i;
          for (i=beatp;i<sem->beatc;i++) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int stop_here=0;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if ((event->op==opposite)&&(i>beatp)) {
                  *beatphi=i-1;
                  goto _done_adjend_up_;
                }
                if (event->op==PS_SEM_OP_NOTEEND) {
                  stop_here=1;
                }
              }
            }
            if (stop_here) {
              *beatphi=i;
              goto _done_adjend_up_;
            }
          }
         _done_adjend_up_:;
          for (i=beatp;i>=0;i--) {
            const struct ps_sem_beat *ibeat=sem->beatv+i;
            const struct ps_sem_event *event=ibeat->eventv;
            int ei=0; for (;ei<ibeat->eventc;ei++,event++) {
              if (event->noteid==noteid) {
                if (event->op==opposite) {
                  *beatplo=i;
                  goto _done_adjend_down_;
                }
              }
            }
          }
         _done_adjend_down_:;
        } break;
        
  }

  return 0;
}

/* Search linearly for event.
 */

static int ps_sem_find_event(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int beatd,uint8_t op,int noteid) {
  while ((beatp>=0)&&(beatp<sem->beatc)) {
    const struct ps_sem_beat *beat=sem->beatv+beatp;
    const struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if ((event->op==op)&&(event->noteid==noteid)) {
        *dstbeatp=beatp;
        *dsteventp=eventp;
        return 0;
      }
    }
    beatp+=beatd;
  }
  return -1;
}

static int ps_sem_find_nearest_event_for_noteid(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int beatd,int noteid) {
  while ((beatp>=0)&&(beatp<sem->beatc)) {
    const struct ps_sem_beat *beat=sem->beatv+beatp;
    const struct ps_sem_event *event=beat->eventv;
    int okeventp=-1;

    /* Moving down, any END event is good. Moving up, any START event is good.
     * If it's the wrong orientation, return it only we don't find something good.
     */
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if (event->noteid==noteid) {
        if (
          ((beatd<0)&&ps_sem_op_is_end(event->op))||
          ((beatd>0)&&ps_sem_op_is_start(event->op))
        ) {
          *dstbeatp=beatp;
          *dsteventp=eventp;
          return 0;
        } else {
          okeventp=eventp;
        }
      }
    }
    
    if (okeventp>=0) {
      *dstbeatp=beatp;
      *dsteventp=okeventp;
      return 0;
    }
    beatp+=beatd;
  }
  return -1;
}

/* Locate partner event.
 */
 
int ps_sem_find_partner(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp) {
  if (!dstbeatp||!dsteventp||!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  if ((eventp<0)||(eventp>=sem->beatv[beatp].eventc)) return -1;
  uint8_t op=sem->beatv[beatp].eventv[eventp].op;
  int noteid=sem->beatv[beatp].eventv[eventp].noteid;
  switch (op) {
    case AKAU_SONG_OP_NOTE: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,1,PS_SEM_OP_NOTEEND,noteid);
    case AKAU_SONG_OP_ADJPITCH: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,1,PS_SEM_OP_ADJPITCHEND,noteid);
    case AKAU_SONG_OP_ADJTRIM: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,1,PS_SEM_OP_ADJTRIMEND,noteid);
    case AKAU_SONG_OP_ADJPAN: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,1,PS_SEM_OP_ADJPANEND,noteid);
    case PS_SEM_OP_NOTEEND: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,-1,AKAU_SONG_OP_NOTE,noteid);
    case PS_SEM_OP_ADJPITCHEND: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,-1,AKAU_SONG_OP_ADJPITCH,noteid);
    case PS_SEM_OP_ADJTRIMEND: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,-1,AKAU_SONG_OP_ADJTRIM,noteid);
    case PS_SEM_OP_ADJPANEND: return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,-1,AKAU_SONG_OP_ADJPAN,noteid);
  }
  *dstbeatp=beatp;
  *dsteventp=eventp;
  return 0;
}

/* Find first event in note chain.
 */
 
int ps_sem_find_note_start(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp) {
  if (!dstbeatp||!dsteventp||!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  const struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return -1;
  const struct ps_sem_event *event=beat->eventv+eventp;
  if (!event->noteid) return -1;
  return ps_sem_find_event(dstbeatp,dsteventp,sem,beatp,-1,AKAU_SONG_OP_NOTE,event->noteid);
}

/* Find next event in note chain.
 */
 
int ps_sem_find_note_next(int *dstbeatp,int *dsteventp,const struct ps_sem *sem,int beatp,int eventp) {
  if (!dstbeatp||!dsteventp||!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  const struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return -1;
  const struct ps_sem_event *event=beat->eventv+eventp;
  if (!event->noteid) return -1;
  if (event->op==PS_SEM_OP_NOTEEND) return -1;
  int noteid=event->noteid;

  /* If we are looking at a START op, proceed along the beat to the next START if present. */
  if (ps_sem_op_is_start(event->op)) {
    int i=eventp+1; for (;i<beat->eventc;i++) {
      const struct ps_sem_event *ievent=beat->eventv+i;
      if (ievent->noteid==noteid) {
        if (ps_sem_op_is_start(ievent->op)) {
          *dstbeatp=beatp;
          *dsteventp=i;
          return 0;
        }
      }
    }
    /* If there are no more STARTs on this beat, look for ENDs on this beat. eventp could be lower than the initial. */
    for (i=0;i<beat->eventc;i++) {
      const struct ps_sem_event *ievent=beat->eventv+i;
      if (ievent->noteid==noteid) {
        if (ps_sem_op_is_end(ievent->op)) {
          *dstbeatp=beatp;
          *dsteventp=i;
          return 0;
        }
      }
    }

  /* If we are looking at an END op, proceed along the beat to the next END if present. */
  } else {
    int i=eventp+1; for (;i<beat->eventc;i++) {
      const struct ps_sem_event *ievent=beat->eventv+i;
      if (ievent->noteid==noteid) {
        if (ps_sem_op_is_end(ievent->op)) {
          *dstbeatp=beatp;
          *dsteventp=i;
          return 0;
        }
      }
    }
  }

  /* Proceed to other beats, returning the first START if any is present, or the first anything. */
  while (1) {
    if (++beatp>=sem->beatc) return -1;
    beat++;
    int end_eventp=-1;
    int i=0; for (;i<beat->eventc;i++) {
      const struct ps_sem_event *ievent=beat->eventv+i;
      if (ievent->noteid==noteid) {
        if (ps_sem_op_is_start(ievent->op)) {
          *dstbeatp=beatp;
          *dsteventp=i;
          return 0;
        }
        if (end_eventp<0) end_eventp=i;
      }
    }
    if (end_eventp>=0) {
      *dstbeatp=beatp;
      *dsteventp=end_eventp;
      return 0;
    }
  }
  return -1;
}

/* Find event in chart.
 */

static int ps_sem_test_objid_default(uint8_t objid,void *userdata) {
  return 1;
}
 
int ps_sem_find_event_in_chart(
  int *eventp,
  const struct ps_sem *sem,
  int beatp,int pitch,int position,
  int (*test_objid)(uint8_t objid,void *userdata),
  void *userdata
) {
  if (!eventp||!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return -1;
  const struct ps_sem_beat *beat=sem->beatv+beatp;
  if (!test_objid) test_objid=ps_sem_test_objid_default;
  
  if ((pitch>=0)&&(pitch<=255)) {
    if (position==0) {
      /* Look for NOTE or ADJ. */
      int i=beat->eventc; while (i-->0) {
        const struct ps_sem_event *event=beat->eventv+i;
        switch (event->op) {
          case AKAU_SONG_OP_NOTE:
          case AKAU_SONG_OP_ADJPITCH:
          case AKAU_SONG_OP_ADJTRIM:
          case AKAU_SONG_OP_ADJPAN: {
              if (event->pitch!=pitch) continue;
            } break;
          default: continue;
        }
        if (!test_objid(event->objid,userdata)) continue;
        *eventp=i;
        return i;
      }
    } else if (position==1) {
      /* Look for NOTEEND or ADJEND. */
      int i=beat->eventc; while (i-->0) {
        const struct ps_sem_event *event=beat->eventv+i;
        switch (event->op) {
          case PS_SEM_OP_NOTEEND:
          case PS_SEM_OP_ADJPITCHEND:
          case PS_SEM_OP_ADJTRIMEND:
          case PS_SEM_OP_ADJPANEND: {
              if (event->pitch!=pitch) continue;
            } break;
          default: continue;
        }
        if (!test_objid(event->objid,userdata)) continue;
        *eventp=i;
        return i;
      }
    }

  } else {
    /* Look for drums. */
    int i=0; for (;i<beat->eventc;i++) {
      const struct ps_sem_event *event=beat->eventv+i;
      if (event->op!=AKAU_SONG_OP_DRUM) continue;
      if (!test_objid(event->objid,userdata)) continue;
      if (!position--) {
        *eventp=i;
        return i;
      }
    }

  }
  return -1;
}

/* Select a reference number for a NOTE command, and store its noteid in the given table.
 * If the table is full, we overwrite whichever slot has the lowest noteid in the hope that it's the earliest note.
 */

static uint8_t ps_sem_select_note_ref(int *noteid_by_ref,int noteid) {
  int noteid_lowest=INT_MAX;
  int position_lowest=1;
  int i=1; for (;i<256;i++) {
    if (!noteid_by_ref[i]) {
      noteid_by_ref[i]=noteid;
      return i;
    }
    if (noteid_by_ref[i]<noteid_lowest) {
      noteid_lowest=noteid_by_ref[i];
      position_lowest=i;
    }
  }
  return position_lowest;
}

static uint8_t ps_sem_search_noteid(const int *noteid_by_ref,int noteid) {
  int i=1; for (;i<256;i++) {
    if (noteid_by_ref[i]==noteid) return i;
  }
  return 0;
}

/* Write NOTE and DRUM commands to song.
 * Skip ADJ commands, since they might require the corresponding NOTE to be written first.
 */

static int ps_sem_write_notes(struct akau_song *song,const struct ps_sem_beat *beat,const struct ps_sem *sem,int beatp,int *noteid_by_ref) {
  const struct ps_sem_event *event=beat->eventv;
  int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
    switch (event->op) {

      case AKAU_SONG_OP_NOTE: {
          int beatpz,eventpz;
          if (ps_sem_find_partner(&beatpz,&eventpz,sem,beatp,eventp)<0) return -1;
          if (beatpz<beatp) return -1;
          union akau_song_command command={.NOTE={
            .op=AKAU_SONG_OP_NOTE,
            .ref=ps_sem_select_note_ref(noteid_by_ref,event->noteid),
            .instrid=event->objid,
            .pitch=event->pitch,
            .trim=event->trim,
            .pan=event->pan,
            .duration=beatpz-beatp+1,
          }};
          if (akau_song_insert_command(song,-1,&command)<0) return -1;
        } break;

      case AKAU_SONG_OP_DRUM: {
          union akau_song_command command={.DRUM={
            .op=AKAU_SONG_OP_DRUM,
            .ref=0,
            .drumid=event->objid,
            .trim=event->trim,
            .pan=event->pan,
          }};
          if (akau_song_insert_command(song,-1,&command)<0) return -1;
        } break;

    }
  }
  return 0;
}

/* Write ADJ commands to song.
 * NOTE and DRUM for this beat are already written.
 */

static int ps_sem_write_adjustments(struct akau_song *song,const struct ps_sem_beat *beat,const struct ps_sem *sem,int beatp,int *noteid_by_ref) {
  const struct ps_sem_event *event=beat->eventv;
  int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
    switch (event->op) {
      case AKAU_SONG_OP_ADJPITCH:
      case AKAU_SONG_OP_ADJTRIM:
      case AKAU_SONG_OP_ADJPAN: {
          int beatpz,eventpz;
          if (ps_sem_find_partner(&beatpz,&eventpz,sem,beatp,eventp)<0) return -1;
          if (beatpz<beatp) return -1;
          const struct ps_sem_event *eventz=sem->beatv[beatpz].eventv+eventpz;
          uint8_t ref=ps_sem_search_noteid(noteid_by_ref,event->noteid);
          if (!ref) return -1;
          union akau_song_command command={.ADJ={
            .op=event->op,
            .ref=ref,
            .duration=beatpz-beatp+1,
          }};
          switch (event->op) {
            case AKAU_SONG_OP_ADJPITCH: command.ADJ.v=eventz->pitch; break;
            case AKAU_SONG_OP_ADJTRIM: command.ADJ.v=eventz->trim; break;
            case AKAU_SONG_OP_ADJPAN: command.ADJ.v=eventz->pan; break;
          }
          if (akau_song_insert_command(song,-1,&command)<0) return -1;
        } break;
    }
  }
  return 0;
}

/* Look for NOTEEND commands and wipe noteid_by_ref accordingly.
 */

static int ps_sem_update_noteid_for_beat(const struct ps_sem_beat *beat,int *noteid_by_ref) {
  const struct ps_sem_event *event=beat->eventv;
  int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
    if (event->op==PS_SEM_OP_NOTEEND) {
      uint8_t ref=ps_sem_search_noteid(noteid_by_ref,event->noteid);
      if (ref) noteid_by_ref[ref]=0;
    }
  }
  return 0;
}

/* Write to song.
 */
 
int ps_sem_write_song(struct akau_song *song,const struct ps_sem *sem) {
  if (!song||!sem) return -1;
  if (akau_song_remove_commands(song,0,akau_song_count_commands(song))<0) return -1;
  int noteid_by_ref[256]={0};
  const struct ps_sem_beat *beat=sem->beatv;
  int beatp=0;
  for (;beatp<sem->beatc;beatp++,beat++) {

    if (ps_sem_write_notes(song,beat,sem,beatp,noteid_by_ref)<0) return -1;
    if (ps_sem_write_adjustments(song,beat,sem,beatp,noteid_by_ref)<0) return -1;
    if (ps_sem_update_noteid_for_beat(beat,noteid_by_ref)<0) return -1;

    union akau_song_command command={.op=AKAU_SONG_OP_BEAT};
    if (akau_song_insert_command(song,-1,&command)<0) return -1;
  }
  return 0;
}

/* Write pitch, trim, or pan to all events for a given noteid at or above a given beat.
 * These are called when we add an adjustment.
 */

static void ps_sem_set_pitch_for_chain(struct ps_sem *sem,int beatp,int noteid,uint8_t pitch) {
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatp<sem->beatc;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      if (event->noteid==noteid) {
        event->pitch=pitch;
      }
    }
  }
}

static void ps_sem_set_trim_for_chain(struct ps_sem *sem,int beatp,int noteid,uint8_t trim) {
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatp<sem->beatc;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      if (event->noteid==noteid) {
        event->trim=trim;
      }
    }
  }
}

static void ps_sem_set_pan_for_chain(struct ps_sem *sem,int beatp,int noteid,int8_t pan) {
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatp<sem->beatc;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      if (event->noteid==noteid) {
        event->pan=pan;
      }
    }
  }
}

/* Read from song.
 */
 
int ps_sem_read_song(struct ps_sem *sem,const struct akau_song *song) {
  if (!sem||!song) return -1;

  union akau_song_command *cmdv=0;
  int cmdc=akau_song_get_all_commands(&cmdv,song);
  if (cmdc<0) return -1;

  int beatc=akau_song_count_beats(song);
  if (beatc<0) return -1;
  if (ps_sem_set_beat_count(sem,0)<0) return -1; // Clear sem.
  if (ps_sem_set_beat_count(sem,beatc)<0) return -1;
  sem->noteid_next=1;

  int noteid_by_ref[256]={0};

  /* See switch case below for AKAU_SONG_OP_BEAT, we must remove the final BEAT command to avoid reporting an error.
   */
  if ((cmdc>0)&&(cmdv[cmdc-1].op==AKAU_SONG_OP_BEAT)) cmdc--;

  int beatp=0,cmdp=0;
  uint8_t opposite;
  for (;cmdp<cmdc;cmdp++) switch (cmdv[cmdp].op) {
    case AKAU_SONG_OP_NOOP: break;
    
    case AKAU_SONG_OP_BEAT: {
        if (++beatp>=sem->beatc) return -1; 
      } break;
    
    case AKAU_SONG_OP_DRUM: {
        struct ps_sem_event *event=ps_sem_beat_new_event(sem->beatv+beatp);
        if (!event) return -1;
        event->op=AKAU_SONG_OP_DRUM;
        event->objid=cmdv[cmdp].DRUM.drumid;
        event->trim=cmdv[cmdp].DRUM.trim;
        event->pan=cmdv[cmdp].DRUM.pan;
        noteid_by_ref[cmdv[cmdp].DRUM.ref]=0;
      } break;

    case AKAU_SONG_OP_NOTE: {
        int beatpz=beatp+cmdv[cmdp].NOTE.duration-1;
        if ((beatpz<beatp)||(beatpz>=sem->beatc)) return -1;
        struct ps_sem_event *event;
        int noteid=sem->noteid_next++;
        noteid_by_ref[cmdv[cmdp].NOTE.ref]=noteid;

        if (!(event=ps_sem_beat_new_event(sem->beatv+beatp))) return -1;
          event->op=AKAU_SONG_OP_NOTE;
          event->noteid=noteid;
          event->objid=cmdv[cmdp].NOTE.instrid;
          event->pitch=cmdv[cmdp].NOTE.pitch;
          event->trim=cmdv[cmdp].NOTE.trim;
          event->pan=cmdv[cmdp].NOTE.pan;

        if (!(event=ps_sem_beat_new_event(sem->beatv+beatpz))) return -1;
          event->op=PS_SEM_OP_NOTEEND;
          event->noteid=noteid;
          event->objid=cmdv[cmdp].NOTE.instrid;
          event->pitch=cmdv[cmdp].NOTE.pitch;
          event->trim=cmdv[cmdp].NOTE.trim;
          event->pan=cmdv[cmdp].NOTE.pan;
          
      } break;

    case AKAU_SONG_OP_ADJPITCH: opposite=PS_SEM_OP_ADJPITCHEND; goto _adj_;
    case AKAU_SONG_OP_ADJTRIM: opposite=PS_SEM_OP_ADJTRIMEND; goto _adj_;
    case AKAU_SONG_OP_ADJPAN: opposite=PS_SEM_OP_ADJPANEND; goto _adj_;
    _adj_: {
        int beatpz=beatp+cmdv[cmdp].ADJ.duration-1;
        if ((beatpz<beatp)||(beatpz>=sem->beatc)) return -1;
        int noteid=noteid_by_ref[cmdv[cmdp].ADJ.ref];
        if (!noteid) break;
        struct ps_sem_event *event;
        
        int srcbeatp,srceventp;
        if (ps_sem_find_nearest_event_for_noteid(&srcbeatp,&srceventp,sem,beatp,-1,noteid)<0) break;
        struct ps_sem_beat *srcbeat=sem->beatv+srcbeatp;
        // Don't take a pointer to srcevent because we might be reallocating it here.

        if (!(event=ps_sem_beat_new_event(sem->beatv+beatp))) return -1;
          memcpy(event,srcbeat->eventv+srceventp,sizeof(struct ps_sem_event));
          event->op=cmdv[cmdp].op;
          event->noteid=noteid;

        /* We already have the NOTEEND op out there, and possibly others.
         * So apply the end value to everything on this note beyond this beat.
         */
        if (!(event=ps_sem_beat_new_event(sem->beatv+beatpz))) return -1;
          memcpy(event,srcbeat->eventv+srceventp,sizeof(struct ps_sem_event));
          event->op=opposite;
          event->noteid=noteid;
          switch (cmdv[cmdp].op) {
            case AKAU_SONG_OP_ADJPITCH: {
                event->pitch=cmdv[cmdp].ADJ.v; 
                ps_sem_set_pitch_for_chain(sem,beatp+1,noteid,event->pitch);
              } break;
            case AKAU_SONG_OP_ADJTRIM: {
                event->trim=cmdv[cmdp].ADJ.v;
                ps_sem_set_trim_for_chain(sem,beatp+1,noteid,event->trim);
              } break;
            case AKAU_SONG_OP_ADJPAN: {
                event->pan=cmdv[cmdp].ADJ.v;
                ps_sem_set_pan_for_chain(sem,beatp+1,noteid,event->pan);
              } break;
          }
          
      } break;

    default: ps_log(EDIT,WARNING,"Unexpected song command 0x%02x at %d/%d.",cmdv[cmdp].op,cmdp,cmdc);
  }

  return 0;
}

/* Dump sem into log.
 */
 
void ps_sem_dump(const struct ps_sem *sem) {
  if (!ps_log_should_print(PS_LOG_DOMAIN_EDIT,PS_LOG_LEVEL_DEBUG)) return;
  ps_log(EDIT,DEBUG,"ps_sem_dump %p...",sem);
  int beatp=0; for (;beatp<sem->beatc;beatp++) {
    const struct ps_sem_beat *beat=sem->beatv+beatp;
    ps_log(EDIT,DEBUG,"  beat %d:",beatp);
    int eventp=0; for (;eventp<beat->eventc;eventp++) {
      const struct ps_sem_event *event=beat->eventv+eventp;
      ps_log(EDIT,DEBUG,
        "    event %d: 0x%02x #%d obj=%d pitch=0x%02x trim=0x%02x pan=%d",
        eventp,event->op,event->noteid,event->objid,event->pitch,event->trim,event->pan
      );
    }
  }
  ps_log(EDIT,DEBUG,"END OF SEM DUMP");
}

/* Read mass objid.
 */
 
int ps_sem_get_objid_in_range(
  uint8_t *drumidlo,uint8_t *drumidhi,uint8_t *instridlo,uint8_t *instridhi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!drumidlo||!drumidhi||!instridlo||!instridhi) return -1;
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;

  *drumidlo=*instridlo=0xff;
  *drumidhi=*instridhi=0x00;

  const struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beat++) {
    const struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            if (event->objid<*drumidlo) *drumidlo=event->objid;
            if (event->objid>*drumidhi) *drumidhi=event->objid;
          } break;
        case AKAU_SONG_OP_NOTE: {
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            if (event->objid<*instridlo) *instridlo=event->objid;
            if (event->objid>*instridhi) *instridhi=event->objid;
          } break;
      }
    }
  }

  return 0;
}

int ps_sem_get_pitch_in_range(
  uint8_t *lo,uint8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!lo||!hi) return -1;
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;

  *lo=0xff;
  *hi=0x00;

  const struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beat++) {
    const struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      switch (event->op) {
        case PS_SEM_OP_ADJPITCHEND:
        case AKAU_SONG_OP_NOTE: {
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            if (event->pitch<*lo) *lo=event->pitch;
            if (event->pitch>*hi) *hi=event->pitch;
          } break;
      }
    }
  }

  return 0;
}

int ps_sem_get_trim_in_range(
  uint8_t *lo,uint8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!lo||!hi) return -1;
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;

  *lo=0xff;
  *hi=0x00;

  const struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beat++) {
    const struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            if (event->trim<*lo) *lo=event->trim;
            if (event->trim>*hi) *hi=event->trim;
          } break;
        case PS_SEM_OP_ADJTRIMEND:
        case AKAU_SONG_OP_NOTE: {
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            if (event->trim<*lo) *lo=event->trim;
            if (event->trim>*hi) *hi=event->trim;
          } break;
      }
    }
  }

  return 0;
}

int ps_sem_get_pan_in_range(
  int8_t *lo,int8_t *hi,
  const struct ps_sem *sem,int beatp,int beatc,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!lo||!hi) return -1;
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;

  *lo=127;
  *hi=-128;

  const struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beat++) {
    const struct ps_sem_event *event=beat->eventv;
    int i=beat->eventc; for (;i-->0;event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            if (event->pan<*lo) *lo=event->pan;
            if (event->pan>*hi) *hi=event->pan;
          } break;
        case PS_SEM_OP_ADJPANEND:
        case AKAU_SONG_OP_NOTE: {
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            if (event->pan<*lo) *lo=event->pan;
            if (event->pan>*hi) *hi=event->pan;
          } break;
      }
    }
  }

  return 0;
}

/* Set one field in all matching commands in a given range.
 */
 
int ps_sem_set_objid_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t drumid,uint8_t instrid,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            event->objid=drumid;
          } break;
        case AKAU_SONG_OP_NOTE: {
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            if (event->objid==instrid) continue; // Already changed.
            int mbeatp,meventp;
            if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) return -1;
            while (1) {
              struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
              mevent->objid=instrid;
              if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
            }
          } break;
      }
    }
  }
  return 0;
}

int ps_sem_set_pitch_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t pitch,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if (!event->noteid) continue;
      if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
      if (ps_sem_adjust_event_pitch(sem,beatp,eventp,pitch)<0) return -1;
    }
  }
  return 0;
}

int ps_sem_set_trim_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  uint8_t trim,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            event->trim=trim;
          } break;
        case AKAU_SONG_OP_NOTE: 
        case AKAU_SONG_OP_ADJPITCH:
        case AKAU_SONG_OP_ADJPAN:
        case AKAU_SONG_OP_ADJTRIM: {
            if (!event->noteid) continue;
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            int mbeatp,meventp;
            if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) return -1;
            while (1) {
              struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
              mevent->trim=trim;
              if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
            }
          } break;
      }
    }
  }
  return 0;
}

int ps_sem_set_pan_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int8_t pan,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      switch (event->op) {
        case AKAU_SONG_OP_DRUM: {
            if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
            event->pan=pan;
          } break;
        case AKAU_SONG_OP_NOTE: 
        case AKAU_SONG_OP_ADJPITCH:
        case AKAU_SONG_OP_ADJPAN:
        case AKAU_SONG_OP_ADJTRIM: {
            if (!event->noteid) continue;
            if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
            int mbeatp,meventp;
            if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) return -1;
            while (1) {
              struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
              mevent->pan=pan;
              if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
            }
          } break;
      }
    }
  }
  return 0;
}

/* Adjust one field in all matching commands in a given range.
 */
 
int ps_sem_adjust_pitch_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if (!event->noteid) continue;
      if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
      int mbeatp,meventp;
      if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) return -1;
      while (1) {
        struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
        int nv=mevent->pitch+d;
        if (nv<0) nv=0; else if (nv>255) nv=255;
        mevent->pitch=nv;
        if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
      }
    }
  }
  return 0;
}

int ps_sem_adjust_trim_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if (event->noteid) {
        if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
      } else {
        if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
      }
      int mbeatp,meventp;
      if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) {
        // Drum or whatever. Start the chain at this position.
        mbeatp=beatp;
        meventp=eventp;
      }
      while (1) {
        struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
        int nv=mevent->trim+d;
        if (nv<0) nv=0; else if (nv>255) nv=255;
        mevent->trim=nv;
        if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
      }
    }
  }
  return 0;
}

int ps_sem_adjust_pan_in_range(
  struct ps_sem *sem,int beatp,int beatc,
  int d,
  int (*filter_drumid)(uint8_t drumid,void *userdata),
  int (*filter_instrid)(uint8_t instrid,void *userdata),
  void *userdata
) {
  if (!sem) return -1;
  if ((beatp<0)||(beatc<0)||(beatc>sem->beatc-beatp)) return -1;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  for (;beatc-->0;beatp++,beat++) {
    struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
      if (event->noteid) {
        if (filter_instrid&&!filter_instrid(event->objid,userdata)) continue;
      } else {
        if (filter_drumid&&!filter_drumid(event->objid,userdata)) continue;
      }
      int mbeatp,meventp;
      if (ps_sem_find_note_start(&mbeatp,&meventp,sem,beatp,eventp)<0) return -1;
      while (1) {
        struct ps_sem_event *mevent=sem->beatv[mbeatp].eventv+meventp;
        int nv=mevent->pan+d;
        if (nv<-128) nv=-128; else if (nv>127) nv=127;
        mevent->pan=nv;
        if (ps_sem_find_note_next(&mbeatp,&meventp,sem,mbeatp,meventp)<0) break;
      }
    }
  }
  return 0;
}
