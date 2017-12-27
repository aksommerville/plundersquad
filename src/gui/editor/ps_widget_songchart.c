/* ps_widget_songchart.c
 * The main editor view for songs.
 * Scrollable view of notes and drums over time.
 * X axis is time, segmented into beats.
 * Y axis is pitch, and a section at the bottom for drums.
 *
 * Children:
 *  [0] Horizontal scrollbar
 *  [1] Vertical scrollbar
 */

#include "ps.h"
#include "../ps_widget.h"
#include "ps_editor.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"
#include "akgl/akgl.h"
#include "video/ps_video.h"
#include "util/ps_geometry.h"
#include "resedit/ps_sem.h"

#define PS_SONGCHART_BEAT_WIDTH       12
#define PS_SONGCHART_NOTE_HEIGHT       6
#define PS_SONGCHART_DRUM_HEIGHT      14

static int ps_songchart_cb_horzbar(struct ps_widget *horzbar,struct ps_widget *widget);
static int ps_songchart_cb_vertbar(struct ps_widget *vertbar,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_songchart {
  struct ps_widget hdr;
  int song_beatc; // Length of song (constantish, reset at model change).

// First beat to examine when drawing.
// Notes may extend rightward, but we have to start drawing them at their beginning.
// So this is generally less than the horizontal scroll. (never greater)
  int view_beatp;
  int view_cmdp;

  // Cache of note lines already drawn, cleared each render.
  int *noteid_visitv;
  int noteid_visitc;
  int noteid_visita;

  // Cache of mintile vertices, cleared each render.
  struct akgl_vtx_mintile *vtxv;
  int vtxc,vtxa;

  uint8_t drum_visibility[32];
  uint8_t instrument_visibility[32];

  int mousex,mousey;
  int dragging;
  int dragbeatp,drageventp,dragpitch;
  int mod_alt;
};

#define WIDGET ((struct ps_widget_songchart*)widget)

/* Delete.
 */

static void _ps_songchart_del(struct ps_widget *widget) {
  if (WIDGET->noteid_visitv) free(WIDGET->noteid_visitv);
  if (WIDGET->vtxv) free(WIDGET->vtxv);
}

/* Initialize.
 */

static int _ps_songchart_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0x404060ff;
  widget->accept_mouse_focus=1;
  widget->accept_mouse_wheel=1;
  widget->drag_verbatim=1;

  memset(WIDGET->drum_visibility,0xff,32);
  memset(WIDGET->instrument_visibility,0xff,32);

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrollbar))) return -1; // horzbar
    if (ps_widget_scrollbar_set_orientation(child,PS_AXIS_HORZ)<0) return -1;
    if (ps_widget_scrollbar_set_callback(child,ps_callback(ps_songchart_cb_horzbar,0,widget))<0) return -1;

  /* Vertbar.
   * We want the initial scroll to have octave 4 in the center.
   * Strictly speaking, we don't know the output height now, but we can cheat a little.
   */
  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrollbar))) return -1;
    if (ps_widget_scrollbar_set_orientation(child,PS_AXIS_VERT)<0) return -1;
    if (ps_widget_scrollbar_set_callback(child,ps_callback(ps_songchart_cb_vertbar,0,widget))<0) return -1;
    if (ps_widget_scrollbar_set_limit(child,256)<0) return -1;
    if (ps_widget_scrollbar_set_value(child,255-0x48)<0) return -1;

  return 0;
}

/* Structural helpers.
 */

static int ps_songchart_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_songchart) return -1;
  if (widget->childc!=2) return -1;
  return 0;
}

static struct ps_widget *ps_songchart_get_horzbar(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_songchart_get_vertbar(const struct ps_widget *widget) {
  return widget->childv[1];
}

/* Draw background.
 */

static uint32_t ps_songchart_row_color_for_pitch(int pitch) {
  if (!(pitch%12)) return 0x00a000ff; // 'a' at any octave.
  if ((pitch>=0x30)&&(pitch<=0x3b)) { // Octave 4 is highlighted.
    if (pitch&1) return 0x408000ff;
    return 0x406000ff;
  }
  if (pitch&1) return 0x008000ff;
  return 0x006000ff;
}

static int ps_songchart_draw_background(struct ps_widget *widget,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  const struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  const struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);

  /* Draw rows for the pitches. */
  int pitch=255-ps_widget_scrollbar_get_value(vertbar);
  int y=0;
  while (y<widget->h) {
    if (pitch<0) {
      if (ps_video_draw_rect(parentx,parenty+y,widget->w,widget->h-parenty-y,0x000000ff)<0) return -1;
      break;
    }
    uint32_t rgba=ps_songchart_row_color_for_pitch(pitch);
    if (ps_video_draw_rect(parentx,parenty+y,widget->w,PS_SONGCHART_NOTE_HEIGHT,rgba)<0) return -1;
    y+=PS_SONGCHART_NOTE_HEIGHT;
    pitch--;
  }

  /* Cover drum area uniformly. */
  if (ps_video_draw_rect(parentx,parenty+vertbar->h,widget->w,widget->h-vertbar->h,0x000080ff)<0) return -1;

  /* Draw transparent columns for the beats. */
  int play_beatp=ps_widget_editsong_get_current_beat(widget);
  int beatp=ps_widget_scrollbar_get_value(horzbar);
  int x=0;
  while (x<widget->w) {
    if (beatp==play_beatp) {
      if (ps_video_draw_rect(parentx+x,parenty,PS_SONGCHART_BEAT_WIDTH,widget->h,0x8080ff40)<0) return -1;
    } else if (beatp&1) {
      if (ps_video_draw_rect(parentx+x,parenty,PS_SONGCHART_BEAT_WIDTH,widget->h,0x80808040)<0) return -1;
    }
    x+=PS_SONGCHART_BEAT_WIDTH;
    beatp++;
  }

  return 0;
}

/* Visited notes cache.
 */

static int ps_songchart_noteid_visitv_search(const struct ps_widget *widget,int noteid) {
  int lo=0,hi=WIDGET->noteid_visitc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (noteid<WIDGET->noteid_visitv[ck]) hi=ck;
    else if (noteid>WIDGET->noteid_visitv[ck]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_songchart_should_draw_noteid(struct ps_widget *widget,int noteid) {
  if (noteid<1) return 0;
  int p=ps_songchart_noteid_visitv_search(widget,noteid);
  if (p>=0) return 0;
  p=-p-1;
  if (WIDGET->noteid_visitc>=WIDGET->noteid_visita) {
    int na=WIDGET->noteid_visita+16;
    if (na>INT_MAX/sizeof(int)) return 0;
    void *nv=realloc(WIDGET->noteid_visitv,sizeof(int)*na);
    if (!nv) return 0;
    WIDGET->noteid_visitv=nv;
    WIDGET->noteid_visita=na;
  }
  memmove(WIDGET->noteid_visitv+p+1,WIDGET->noteid_visitv+p,sizeof(int)*(WIDGET->noteid_visitc-p));
  WIDGET->noteid_visitc++;
  WIDGET->noteid_visitv[p]=noteid;
  return 1;
}

/* Draw one note line.
 */

static int ps_songchart_populate_line_vertex(
  struct akgl_vtx_raw *vtx,
  struct ps_widget *widget,const struct ps_sem *sem,
  int beatp,int eventp,
  int parentx,int parenty,
  int beata,int pitchz
) {
  const struct ps_sem_event *event=sem->beatv[beatp].eventv+eventp;
  vtx->x=parentx+(beatp-beata)*PS_SONGCHART_BEAT_WIDTH+(PS_SONGCHART_BEAT_WIDTH>>2);
  vtx->y=parenty+(pitchz-event->pitch)*PS_SONGCHART_NOTE_HEIGHT+(PS_SONGCHART_NOTE_HEIGHT>>1);
  vtx->r=0xff; vtx->g=0xff; vtx->b=0x00; vtx->a=0xff;
  switch (event->op) {
    case PS_SEM_OP_NOTEEND:
    case PS_SEM_OP_ADJPITCHEND:
    case PS_SEM_OP_ADJTRIMEND:
    case PS_SEM_OP_ADJPANEND: {
        vtx->x+=PS_SONGCHART_BEAT_WIDTH>>1;
      } break;
  }
  return 0;
}

static int ps_songchart_draw_line(struct ps_widget *widget,const struct ps_sem *sem,int parentx,int parenty,int beatp,int eventp,int beata,int pitchz) {
  const int vtxa=16;
  struct akgl_vtx_raw vtxv[vtxa];
  int vtxc=0;

  if (ps_sem_find_note_start(&beatp,&eventp,sem,beatp,eventp)<0) return -1;
  while (1) {
    if (vtxc>=vtxa) break;
    if (ps_songchart_populate_line_vertex(vtxv+vtxc++,widget,sem,beatp,eventp,parentx,parenty,beata,pitchz)<0) return -1;
    if (ps_sem_find_note_next(&beatp,&eventp,sem,beatp,eventp)<0) break;
  }

  if (ps_video_draw_line_strip(vtxv,vtxc)<0) return -1;
  return 0;
}

/* Draw note lines.
 */

static int ps_songchart_draw_lines(struct ps_widget *widget,const struct ps_sem *sem,int parentx,int parenty) {
  WIDGET->noteid_visitc=0;
  parentx+=widget->x;
  parenty+=widget->y;
  const struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  const struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);
  int beatp=ps_widget_scrollbar_get_value(horzbar);
  int beata=beatp;
  int beatz=beatp+ps_widget_scrollbar_get_visible_size(horzbar)+1;
  if (beatz>=sem->beatc) beatz=sem->beatc-1;
  int pitchz=255-ps_widget_scrollbar_get_value(vertbar);
  while (beatp<=beatz) {
    const struct ps_sem_beat *beat=sem->beatv+beatp;
    const struct ps_sem_event *event=beat->eventv;
    int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {

      // We don't actually know that it's an instrument command, but those are the only ones we draw anyway.
      if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,event->objid)) continue;

      if (ps_songchart_should_draw_noteid(widget,event->noteid)) {
        if (ps_songchart_draw_line(widget,sem,parentx,parenty,beatp,eventp,beata,pitchz)<0) return -1;
      }
    }
    beatp++;
  }
  return 0;
}

/* Helpers for mintile vertex buffer.
 */

static struct akgl_vtx_mintile *ps_songchart_new_vtx_mintile(struct ps_widget *widget) {
  if (WIDGET->vtxc>=WIDGET->vtxa) {
    int na=WIDGET->vtxa+16;
    if (na>INT_MAX/sizeof(struct akgl_vtx_mintile)) return 0;
    void *nv=realloc(WIDGET->vtxv,sizeof(struct akgl_vtx_mintile)*na);
    if (!nv) return 0;
    WIDGET->vtxv=nv;
    WIDGET->vtxa=na;
  }
  return WIDGET->vtxv+WIDGET->vtxc++;
}

/* Draw drum.
 */

static int ps_songchart_draw_drum(struct ps_widget *widget,int x,int y,int position,uint8_t drumid) {
  struct akgl_vtx_mintile *vtx=ps_songchart_new_vtx_mintile(widget);
  if (!vtx) return -1;
  vtx->x=x+2;
  vtx->y=y+3;
  switch (position) {
    case 0: vtx->x+=1; vtx->y+=PS_SONGCHART_DRUM_HEIGHT>>1; break;
    case 1: vtx->x+=(PS_SONGCHART_BEAT_WIDTH>>1)+1; vtx->y+=PS_SONGCHART_DRUM_HEIGHT>>1; break;
    case 2: vtx->x+=1; break;
    case 3: vtx->x+=(PS_SONGCHART_BEAT_WIDTH>>1)+1; break;
    default: return 0;
  }
  if (drumid<13) vtx->tileid=0x13+drumid;
  else vtx->tileid=0x1f;
  return 0;
}

/* Draw note.
 */

static int ps_songchart_draw_note(struct ps_widget *widget,int x,int y,uint8_t tileid,int position,uint8_t instrid) {
  struct akgl_vtx_mintile *vtx=ps_songchart_new_vtx_mintile(widget);
  if (!vtx) return -1;
  vtx->x=x+3;
  vtx->y=y+3;
  vtx->tileid=tileid;
  if (position) vtx->x+=PS_SONGCHART_BEAT_WIDTH>>1;
  return 0;
}

/* Draw beats.
 */

static int ps_songchart_draw_beat(struct ps_widget *widget,const struct ps_sem *sem,int beatp,int x,int parenty,uint8_t pitcha,uint8_t pitchz,int drumtop) {
  const struct ps_sem_beat *beat=sem->beatv+beatp;
  const struct ps_sem_event *event=beat->eventv;
  int drumc=0;
  int eventp=0; for (;eventp<beat->eventc;eventp++,event++) {
    switch (event->op) {

      case AKAU_SONG_OP_DRUM: {
          if (!PS_EDITSONG_TEST_VOICE(WIDGET->drum_visibility,event->objid)) continue;
          if (ps_songchart_draw_drum(widget,x,parenty+drumtop,drumc++,event->objid)<0) return -1;
        } break;

      case AKAU_SONG_OP_NOTE: {
          if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,event->objid)) continue;
          if ((event->pitch<pitcha)||(event->pitch>pitchz)) break;
          int y=(pitchz-event->pitch)*PS_SONGCHART_NOTE_HEIGHT;
          if (ps_songchart_draw_note(widget,x,parenty+y,0x06,0,event->objid)<0) return -1;
        } break;
        
      case AKAU_SONG_OP_ADJPITCH:
      case AKAU_SONG_OP_ADJTRIM:
      case AKAU_SONG_OP_ADJPAN: {
          if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,event->objid)) continue;
          if ((event->pitch<pitcha)||(event->pitch>pitchz)) break;
          int y=(pitchz-event->pitch)*PS_SONGCHART_NOTE_HEIGHT;
          if (ps_songchart_draw_note(widget,x,parenty+y,0x07,0,event->objid)<0) return -1;
        } break;
        
      case PS_SEM_OP_NOTEEND: {
          if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,event->objid)) continue;
          if ((event->pitch<pitcha)||(event->pitch>pitchz)) break;
          int y=(pitchz-event->pitch)*PS_SONGCHART_NOTE_HEIGHT;
          if (ps_songchart_draw_note(widget,x,parenty+y,0x08,1,event->objid)<0) return -1;
        } break;
        
      case PS_SEM_OP_ADJPITCHEND:
      case PS_SEM_OP_ADJTRIMEND:
      case PS_SEM_OP_ADJPANEND: {
          if (!PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,event->objid)) continue;
          if ((event->pitch<pitcha)||(event->pitch>pitchz)) break;
          int y=(pitchz-event->pitch)*PS_SONGCHART_NOTE_HEIGHT;
          if (ps_songchart_draw_note(widget,x,parenty+y,0x07,1,event->objid)<0) return -1;
        } break;
    }
  }
  return 0;
}

static int ps_songchart_draw_beats(struct ps_widget *widget,const struct ps_sem *sem,int parentx,int parenty) {
  parentx+=widget->x;
  parenty+=widget->y;
  const struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  const struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);
  int pitchz=255-ps_widget_scrollbar_get_value(vertbar);
  int pitcha=pitchz-ps_widget_scrollbar_get_visible_size(vertbar);
  if (pitcha<0) pitcha=0;
  int beatp=ps_widget_scrollbar_get_value(horzbar);
  int x=0;
  int drumtop=horzbar->y-PS_SONGCHART_DRUM_HEIGHT;
  WIDGET->vtxc=0;
  while ((beatp<sem->beatc)&&(x<widget->w)) {
    if (ps_songchart_draw_beat(widget,sem,beatp,parentx+x,parenty,pitcha,pitchz,drumtop)<0) return -1;
    beatp++;
    x+=PS_SONGCHART_BEAT_WIDTH;
  }
  if (ps_video_draw_mintile(WIDGET->vtxv,WIDGET->vtxc,0x01)<0) return -1;
  WIDGET->vtxc=0;
  return 0;
}

/* Draw, main.
 */

static int _ps_songchart_draw(struct ps_widget *widget,int parentx,int parenty) {
  if (ps_songchart_draw_background(widget,parentx,parenty)<0) return -1;

  const struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (sem) {

    const struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
    const struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);  
    if (akgl_scissor(parentx+widget->x,parenty+widget->y,vertbar->x,horzbar->y)<0) return -1;

    if (
      (ps_songchart_draw_lines(widget,sem,parentx,parenty)<0)||
      (ps_songchart_draw_beats(widget,sem,parentx,parenty)<0)
    ) {
      akgl_scissor_none();
      return -1;
    }
    akgl_scissor_none();
  }
  
  if (ps_widget_draw_children(widget,parentx,parenty)<0) return -1;
  return 0;
}

/* Measure.
 */

static int _ps_songchart_measure(int *w,int *h,struct ps_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
  return 0;
}

/* Pack.
 */

static int _ps_songchart_pack(struct ps_widget *widget) {
  int chw,chh;
  if (ps_songchart_obj_validate(widget)<0) return -1;
  struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);

  /* Horzbar gets full width and desired height, at my bottom. */
  if (ps_widget_measure(&chw,&chh,horzbar,widget->w,widget->h)<0) return -1;
  horzbar->x=0;
  horzbar->y=widget->h-chh;
  horzbar->w=widget->w;
  horzbar->h=chh;

  /* Drums section goes above horzbar, at a fixed height. */
  int notesheight=widget->h-horzbar->h-PS_SONGCHART_DRUM_HEIGHT;

  /* Vertbar gets desired width and remaining height (above drums section), at my right. */
  if (ps_widget_measure(&chw,&chh,vertbar,widget->w,notesheight)<0) return -1;
  vertbar->x=widget->w-chw;
  vertbar->y=0;
  vertbar->w=chw;
  vertbar->h=notesheight;

  /* Deliver visible sizes to scroll bars. */
  int vispitchc=notesheight/PS_SONGCHART_NOTE_HEIGHT;
  if (ps_widget_scrollbar_set_visible_size(vertbar,vispitchc)<0) return -1;
  int visbeatc=widget->w/PS_SONGCHART_BEAT_WIDTH;
  if (ps_widget_scrollbar_set_visible_size(horzbar,visbeatc)<0) return -1;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */

static int _ps_songchart_update(struct ps_widget *widget) {
  return 0;
}

/* Transform mouse position into logical beat and pitch.
 */

static int ps_songchart_sem_coords_from_point(int *beatp,int *pitch,int *position,const struct ps_widget *widget,int x,int y) {
  const struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  const struct ps_widget *vertbar=ps_songchart_get_vertbar(widget);
  const struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (!sem) return -1;
  
  *beatp=(x/PS_SONGCHART_BEAT_WIDTH)+ps_widget_scrollbar_get_value(horzbar);
  if ((*beatp<0)||(*beatp>=sem->beatc)) return -1;
  int subx=x%PS_SONGCHART_BEAT_WIDTH;

  // Finding pitch and position depends on whether we're in the drum zone.
  int drumtop=horzbar->y-PS_SONGCHART_DRUM_HEIGHT;
  if (y>=drumtop) {
    *pitch=-1;
    int suby=(y-drumtop);
    if (subx<PS_SONGCHART_BEAT_WIDTH>>1) *position=0;
    else *position=1;
    if (suby<PS_SONGCHART_DRUM_HEIGHT>>1) (*position)+=2;
  } else {
    *pitch=(255-ps_widget_scrollbar_get_value(vertbar))-(y/PS_SONGCHART_NOTE_HEIGHT);
    if ((*pitch<0)||(*pitch>255)) return 0;
    if (subx<PS_SONGCHART_BEAT_WIDTH>>1) *position=0;
    else *position=1;
  }

  return 0;
}

/* Transform mouse position into SEM coordinates.
 * Returns >0 and populates (beatp,eventp) if something is found.
 * Returns 0 if no event found.
 * In that case, (beatp) is valid and (eventp) is the pitch or <0 for drum zone.
 * Returns <0 if we can't do even that.
 */

static int ps_songchart_cb_test_drumid(uint8_t objid,void *userdata) {
  const struct ps_widget *widget=userdata;
  return PS_EDITSONG_TEST_VOICE(WIDGET->drum_visibility,objid);
}

static int ps_songchart_cb_test_instrid(uint8_t objid,void *userdata) {
  const struct ps_widget *widget=userdata;
  return PS_EDITSONG_TEST_VOICE(WIDGET->instrument_visibility,objid);
}

static int ps_songchart_find_event_at_location(int *beatp,int *eventp,const struct ps_widget *widget,int x,int y) {
  const struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (!sem) return -1;

  int pitch,position;
  int (*cb_test)(uint8_t objid,void *userdata)=0;
  if (ps_songchart_sem_coords_from_point(beatp,&pitch,&position,widget,x,y)<0) {
    *eventp=pitch;
    return 0;
  }
  if (pitch<0) cb_test=ps_songchart_cb_test_drumid;
  else cb_test=ps_songchart_cb_test_instrid;

  // Let SEM do the heavy lifting.
  if (ps_sem_find_event_in_chart(eventp,sem,*beatp,pitch,position,cb_test,(void*)widget)<0) {
    *eventp=pitch;
    return 0;
  }
  
  return 1;
}

/* Get drum or instrument ID for new event creation.
 */

static int ps_songchart_get_first_visible_voice(const uint8_t *bits) {
  int major=0; for (;major<32;major++) {
    if (!bits[major]) continue;
    int minor=0; for (;minor<8;minor++) {
      if (bits[major]&(1<<minor)) return (major<<3)|minor;
    }
  }
  return -1;
}

/* Create new event.
 */

static int ps_songchart_create_event(struct ps_widget *widget,int beatp,int pitch) {
  struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (!sem) return -1;
  if ((beatp<0)||(beatp>=sem->beatc)) return 0;

  if (pitch<0) {
    int drumid=ps_songchart_get_first_visible_voice(WIDGET->drum_visibility);
    if (drumid<0) return 0; // No selectable drum id
    uint8_t trim=0x60;
    int8_t pan=0;
    if (ps_sem_add_drum(sem,beatp,drumid,trim,pan)<0) return -1;

  } else {
    int instrid=ps_songchart_get_first_visible_voice(WIDGET->instrument_visibility);
    if (instrid<0) return 0; // No selectable instrument id
    uint8_t trim=0x60;
    int8_t pan=0;
    if (ps_sem_add_note(sem,beatp,instrid,pitch,trim,pan)<0) return -1;

  }
  return 0;
}

/* Mouse motion.
 */

static int _ps_songchart_mousemotion(struct ps_widget *widget,int x,int y) {
  WIDGET->mousex=x;
  WIDGET->mousey=y;

  if (WIDGET->dragging) {
    int nbeatp,npitch,position;
    if (ps_songchart_sem_coords_from_point(&nbeatp,&npitch,&position,widget,x,y)>=0) {
      struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
      
      if (nbeatp!=WIDGET->dragbeatp) {
        int neventp=ps_sem_move_event(sem,WIDGET->dragbeatp,WIDGET->drageventp,nbeatp);
        if (neventp>=0) {
          WIDGET->dragbeatp=nbeatp;
          WIDGET->drageventp=neventp;
        }
      }
      
      if (npitch!=WIDGET->dragpitch) {
        ps_sem_adjust_event_pitch(sem,WIDGET->dragbeatp,WIDGET->drageventp,npitch);
        WIDGET->dragpitch=npitch;
      }
    }
  }
  
  return 0;
}

/* Begin interactive editing of event.
 */

static int ps_songchart_edit_event(struct ps_widget *widget,int beatp,int eventp) {
  struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (!sem) return 0;
  if ((beatp<0)||(beatp>=sem->beatc)) return 0;
  struct ps_sem_beat *beat=sem->beatv+beatp;
  if ((eventp<0)||(eventp>=beat->eventc)) return 0;
  struct ps_sem_event *event=beat->eventv+eventp;

  struct ps_widget *root=ps_widget_get_root(widget);
  struct ps_widget *dialogue=ps_widget_spawn(root,&ps_widget_type_songevent);
  if (!dialogue) return -1;

  if (ps_widget_songevent_setup(dialogue,sem,beatp,eventp)<0) return -1;
  
  switch (event->op) {//XXX
    case AKAU_SONG_OP_DRUM:
    case AKAU_SONG_OP_NOTE:
    case AKAU_SONG_OP_ADJPITCH:
    case AKAU_SONG_OP_ADJTRIM:
    case AKAU_SONG_OP_ADJPAN:
    case PS_SEM_OP_NOTEEND:
    case PS_SEM_OP_ADJPITCHEND:
    case PS_SEM_OP_ADJTRIMEND:
    case PS_SEM_OP_ADJPANEND:;
  }

  if (ps_widget_pack(root)<0) return -1;
  return 0;
}

/* Mouse button.
 */

static int _ps_songchart_mousebutton(struct ps_widget *widget,int btnid,int value) {
  //TODO create adjustment events
  //TODO clean this up!
  if (btnid==1) { // left button
    if (value) { // left button down
      int beatp,eventp;
      int err=ps_songchart_find_event_at_location(&beatp,&eventp,widget,WIDGET->mousex,WIDGET->mousey);
      if (err>0) {
        if (WIDGET->mod_alt) { // left button down in event, with Alt == delete
          struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
          if (ps_sem_remove_event(sem,beatp,eventp)<0) return -1;
        } else { // left button down in event, without Alt == move
          WIDGET->dragging=1;
          WIDGET->dragbeatp=beatp;
          WIDGET->drageventp=eventp;
          WIDGET->dragpitch=-1; // Let mousemotion figure it out.
        }
      } else if (!err) {
        if (!WIDGET->mod_alt) { // left button down outside event, without Alt == new event
          if (ps_songchart_create_event(widget,beatp,eventp)<0) return -1;
        }
      }
    } else { // left button up
      WIDGET->dragging=0;
    }
  } else if (btnid==3) { // right button
    if (value) { // right button down
      int beatp,eventp;
      if (ps_songchart_find_event_at_location(&beatp,&eventp,widget,WIDGET->mousex,WIDGET->mousey)>0) {
        if (ps_songchart_edit_event(widget,beatp,eventp)<0) return -1;
      }
    }
  }
  return 0;
}

/* Mouse wheel.
 */

static int _ps_songchart_mousewheel(struct ps_widget *widget,int dx,int dy) {
  if (dx) {
    if (ps_widget_scrollbar_adjust(ps_songchart_get_horzbar(widget),dx)<0) return -1;
  }
  if (dy) {
    if (ps_widget_scrollbar_adjust(ps_songchart_get_vertbar(widget),dy)<0) return -1;
  }
  return 0;
}

static int _ps_songchart_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  //ps_log(EDIT,TRACE,"%s %d[U+%x]=%d",__func__,keycode,codepoint,value);
  switch (keycode) {
    case 0x000700e2: WIDGET->mod_alt=value; break; // Left Alt/Option
  }
  return 0;
}

static int _ps_songchart_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_songchart={

  .name="songchart",
  .objlen=sizeof(struct ps_widget_songchart),

  .del=_ps_songchart_del,
  .init=_ps_songchart_init,

  .draw=_ps_songchart_draw,
  .measure=_ps_songchart_measure,
  .pack=_ps_songchart_pack,

  .update=_ps_songchart_update,
  .mousemotion=_ps_songchart_mousemotion,
  .mousebutton=_ps_songchart_mousebutton,
  .mousewheel=_ps_songchart_mousewheel,
  .key=_ps_songchart_key,
  .userinput=_ps_songchart_userinput,

};

/* Callbacks.
 */
 
static int ps_songchart_cb_horzbar(struct ps_widget *horzbar,struct ps_widget *widget) {
  int value=ps_widget_scrollbar_get_value(horzbar);
  return 0;
}

static int ps_songchart_cb_vertbar(struct ps_widget *vertbar,struct ps_widget *widget) {
  int value=ps_widget_scrollbar_get_value(vertbar);
  return 0;
}

/* Rebuild in response to model change.
 */
 
int ps_widget_songchart_rebuild(struct ps_widget *widget) {
  if (ps_songchart_obj_validate(widget)<0) return -1;
  struct ps_widget *horzbar=ps_songchart_get_horzbar(widget);
  
  const struct ps_sem *sem=ps_widget_editsong_get_sem(widget);
  if (sem) {
    int beatc=sem->beatc;
    if (ps_widget_scrollbar_set_limit(horzbar,beatc)<0) return -1;
    
  } else {
    if (ps_widget_scrollbar_set_limit(horzbar,0)<0) return -1;

  }
  return 0;
}

/* Reassess voice visibility.
 */
 
int ps_widget_songchart_voice_visibility_changed(struct ps_widget *widget) {
  if (ps_songchart_obj_validate(widget)<0) return -1;
  if (ps_widget_editsong_get_voice_visibility(WIDGET->drum_visibility,WIDGET->instrument_visibility,widget)<0) return -1;
  return 0;
}
