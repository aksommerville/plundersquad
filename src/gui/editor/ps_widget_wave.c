/* ps_widget_wave.c
 * Modal for editing a wave, for IPCM voices or song instruments.
 *
 * Children:
 *  [0] OK button
 *  [1] Cancel button
 *  [2] Wave visualization
 *  [3] Presets (sine,square,saw,noise). Disabled in USAGE_INSTRUMENT.
 *  [4] Harmonics (16 sliders). Disabled in USAGE_IPCM with a preset selected.
 *  [5] Envelope (line chart with moveable points). Disabled in USAGE_IPCM.
 */

#include "ps.h"
#include "gui/ps_widget.h"
#include "ps_editor.h"
#include "util/ps_text.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "akau/akau.h"

static int ps_wave_cb_ok(struct ps_widget *button,struct ps_widget *widget);
static int ps_wave_cb_cancel(struct ps_widget *button,struct ps_widget *widget);
static int ps_wave_cb_preset(struct ps_widget *scrolllist,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_wave {
  struct ps_widget hdr;
  int usage;
  void *serial;
  int serialc;
  struct ps_wave_ui_model model;
  struct akau_fpcm *fpcm; // generated on demand; null if dirty
  struct akau_instrument *instrument; // "
  int play_chanid;
  int play_pitch;
  struct ps_callback cb;
  int refnum1;
};

#define WIDGET ((struct ps_widget_wave*)widget)

/* Delete.
 */

static void _ps_wave_del(struct ps_widget *widget) {

  akau_mixer_stop_all(akau_get_mixer(),10000);

  if (WIDGET->serial) free(WIDGET->serial);
  akau_fpcm_del(WIDGET->fpcm);
  akau_instrument_del(WIDGET->instrument);
  ps_callback_cleanup(&WIDGET->cb);
}

/* Initialize.
 */

static int _ps_wave_init(struct ps_widget *widget) {
  struct ps_widget *child;

  widget->bgrgba=0x8000c0ff;
  widget->accept_keyboard_focus=1;

  WIDGET->play_chanid=-1;
  WIDGET->play_pitch=-1;

  WIDGET->model.preset=PS_WIDGET_WAVE_PRESET_HARMONICS;
  WIDGET->model.coefv[0]=1.0;
  WIDGET->model.attack_time=10;
  WIDGET->model.attack_trim=0xff;
  WIDGET->model.drawback_time=10;
  WIDGET->model.drawback_trim=0xc0;
  WIDGET->model.decay_time=50;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // OK
  if (ps_widget_button_set_text(child,"OK",2)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_wave_cb_ok,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_button))) return -1; // Cancel
  if (ps_widget_button_set_text(child,"Cancel",6)<0) return -1;
  if (ps_widget_button_set_callback(child,ps_callback(ps_wave_cb_cancel,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_waveview))) return -1; // Visualization

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_scrolllist))) return -1; // Presets
  child->bgrgba=0xa0a0a0ff;
  if (ps_widget_scrolllist_add_label(child,"Sine",4)<0) return -1;
  if (ps_widget_scrolllist_add_label(child,"Square",6)<0) return -1;
  if (ps_widget_scrolllist_add_label(child,"Saw",3)<0) return -1;
  if (ps_widget_scrolllist_add_label(child,"Noise",5)<0) return -1;
  if (ps_widget_scrolllist_add_label(child,"Harmonics",9)<0) return -1;
  if (ps_widget_scrolllist_enable_selection(child,ps_callback(ps_wave_cb_preset,0,widget))<0) return -1;
  if (ps_widget_scrolllist_set_selection(child,WIDGET->model.preset)<0) return -1;

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_harmonics))) return -1; // Harmonics

  if (!(child=ps_widget_spawn(widget,&ps_widget_type_waveenv))) return -1; // Envelope

  return 0;
}

/* Structure access and validation.
 */

static int ps_wave_obj_validate(const struct ps_widget *widget) {
  if (!widget) return -1;
  if (widget->type!=&ps_widget_type_wave) return -1;
  if (widget->childc!=6) return -1;
  return 0;
}

static struct ps_widget *ps_wave_get_ok_button(const struct ps_widget *widget) {
  return widget->childv[0];
}

static struct ps_widget *ps_wave_get_cancel_button(const struct ps_widget *widget) {
  return widget->childv[1];
}

static struct ps_widget *ps_wave_get_visualization(const struct ps_widget *widget) {
  return widget->childv[2];
}

static struct ps_widget *ps_wave_get_presets(const struct ps_widget *widget) {
  return widget->childv[3];
}

static struct ps_widget *ps_wave_get_harmonics(const struct ps_widget *widget) {
  return widget->childv[4];
}

static struct ps_widget *ps_wave_get_envelope(const struct ps_widget *widget) {
  return widget->childv[5];
}

/* Pack.
 */

static int ps_wave_place_buttons(struct ps_widget *widget) {
  struct ps_widget *ok=ps_wave_get_ok_button(widget);
  struct ps_widget *cancel=ps_wave_get_cancel_button(widget);

  int okw,okh,cancelw,cancelh;
  if (ps_widget_measure(&okw,&okh,ok,widget->w,widget->h)<0) return -1;
  if (ps_widget_measure(&cancelw,&cancelh,cancel,widget->w-okw,widget->h)<0) return -1;

  /* Both buttons get whichever height is larger (in reality, they will always be the same).
   * Each gets its preferred width.
   * "OK" in the bottom right corner.
   * "Cancel" immediately left of that.
   */
  int btnh=(okh>cancelh)?okh:cancelh;
  ok->x=widget->w-okw;
  ok->y=widget->h-btnh;
  ok->w=okw;
  ok->h=btnh;
  cancel->x=ok->x-cancelw;
  cancel->y=ok->y;
  cancel->w=cancelw;
  cancel->h=ok->h;

  return 0;
}

static int ps_wave_place_visualization(struct ps_widget *widget) {
  struct ps_widget *ok=ps_wave_get_ok_button(widget);
  struct ps_widget *visualization=ps_wave_get_visualization(widget);

  /* Visualization takes the full width and half of the available height above buttons.
   * OK and Cancel buttons are the same height.
   */
  visualization->x=0;
  visualization->w=widget->w;
  visualization->h=ok->y>>1;
  visualization->y=ok->y-visualization->h;

  return 0;
}

static int ps_wave_place_inputs(struct ps_widget *widget) {
  struct ps_widget *visualization=ps_wave_get_visualization(widget);
  struct ps_widget *envelope=ps_wave_get_envelope(widget);
  struct ps_widget *presets=ps_wave_get_presets(widget);
  struct ps_widget *harmonics=ps_wave_get_harmonics(widget);

  /* The three input widgets occupy the full width and all height above the visualization.
   * Harmonics take 2/3 of the width, on the right.
   * Envelope or preset takes the left third.
   * Only one of (envelope,presets) is enabled, the other has zero size.
   */
  envelope->x=0;
  envelope->y=0;
  presets->x=0;
  presets->y=0;
  harmonics->x=widget->w/3;
  harmonics->w=widget->w-harmonics->x;
  harmonics->y=0;
  harmonics->h=visualization->y;
  
  switch (WIDGET->usage) {
    case PS_WIDGET_WAVE_USAGE_IPCM: {
        envelope->w=0;
        envelope->h=0;
        presets->w=harmonics->x;
        presets->h=harmonics->h;
      } break;
    case PS_WIDGET_WAVE_USAGE_INSTRUMENT: {
        envelope->w=harmonics->x;
        envelope->h=harmonics->h;
        presets->w=0;
        presets->h=0;
      } break;
    default: return -1;
  }

  return 0;
}

static int _ps_wave_pack(struct ps_widget *widget) {
  if (ps_wave_obj_validate(widget)<0) return -1;

  if (ps_wave_place_buttons(widget)<0) return -1;
  if (ps_wave_place_visualization(widget)<0) return -1;
  if (ps_wave_place_inputs(widget)<0) return -1;
  
  int i=0; for (;i<widget->childc;i++) {
    struct ps_widget *child=widget->childv[i];
    if (ps_widget_pack(child)<0) return -1;
  }
  return 0;
}

/* Play the current instrument.
 */

static int ps_wave_play(struct ps_widget *widget,uint8_t pitch) {
  struct akau_instrument *instrument=ps_widget_wave_get_instrument(widget);
  if (!instrument) return -1;
  struct akau_mixer *mixer=akau_get_mixer();
  if (!mixer) return -1;

  int duration=44100*5;

  if (WIDGET->play_chanid>=0) {
    if (pitch==WIDGET->play_pitch) return 0;
    akau_mixer_stop_channel(mixer,WIDGET->play_chanid);
    WIDGET->play_chanid=-1;
  }

  int chanid=akau_mixer_play_note(mixer,instrument,pitch,0x40,0,duration);
  if (chanid<0) return -1;
  WIDGET->play_chanid=chanid;
  WIDGET->play_pitch=pitch;

  return 0;
}

static int ps_wave_stop(struct ps_widget *widget) {
  if (WIDGET->play_chanid>=0) {
    struct akau_mixer *mixer=akau_get_mixer();
    akau_mixer_stop_channel(mixer,WIDGET->play_chanid);
    WIDGET->play_chanid=-1;
    WIDGET->play_pitch=-1;
  }
  return 0;
}

/* Primitive input events.
 */

static int _ps_wave_key(struct ps_widget *widget,int keycode,int codepoint,int value) {
  if (value) switch (codepoint) {
  
    case 'q': return ps_wave_play(widget,0x20);
    case 'a': return ps_wave_play(widget,0x21);
    case 'w': return ps_wave_play(widget,0x22);
    case 's': return ps_wave_play(widget,0x23);
    case 'e': return ps_wave_play(widget,0x24);
    case 'd': return ps_wave_play(widget,0x25);
    case 'r': return ps_wave_play(widget,0x26);
    case 'f': return ps_wave_play(widget,0x27);
    case 't': return ps_wave_play(widget,0x28);
    case 'g': return ps_wave_play(widget,0x29);
    case 'y': return ps_wave_play(widget,0x2a);
    case 'h': return ps_wave_play(widget,0x2b);
    case 'u': return ps_wave_play(widget,0x2c);
    case 'j': return ps_wave_play(widget,0x2d);
    case 'i': return ps_wave_play(widget,0x2e);
    case 'k': return ps_wave_play(widget,0x2f);
    case 'o': return ps_wave_play(widget,0x30);
    case 'l': return ps_wave_play(widget,0x31);
    case 'p': return ps_wave_play(widget,0x32);
    case ';': return ps_wave_play(widget,0x33);
    case '[': return ps_wave_play(widget,0x34);
    case '\'':return ps_wave_play(widget,0x35);
    case ']': return ps_wave_play(widget,0x36);
    
    case 'z': return ps_wave_play(widget,0x00);
    case 'x': return ps_wave_play(widget,0x0c);
    case 'c': return ps_wave_play(widget,0x18);
    case 'v': return ps_wave_play(widget,0x24);
    case 'b': return ps_wave_play(widget,0x30);
    case 'n': return ps_wave_play(widget,0x3c);
    case 'm': return ps_wave_play(widget,0x48);
    case ',': return ps_wave_play(widget,0x54);
    case '.': return ps_wave_play(widget,0x60);
    case '/': return ps_wave_play(widget,0x6c);
    
  } else {
    return ps_wave_stop(widget);
  }
  return 0;
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_wave={

  .name="wave",
  .objlen=sizeof(struct ps_widget_wave),

  .del=_ps_wave_del,
  .init=_ps_wave_init,

  .pack=_ps_wave_pack,

  .key=_ps_wave_key,

};

/* Set up for IPCM usage.
 */

static int ps_wave_setup_ipcm(struct ps_widget *widget) {
  WIDGET->usage=PS_WIDGET_WAVE_USAGE_IPCM;
  return 0;
}

/* Set up for instrument usage.
 */

static int ps_wave_setup_instrument(struct ps_widget *widget) {
  WIDGET->usage=PS_WIDGET_WAVE_USAGE_INSTRUMENT;
  return 0;
}

/* Replace serial data (no other action).
 */

static int ps_wave_replace_serial(struct ps_widget *widget,const void *src,int srcc) {
  if (srcc<0) return -1;
  if (!src&&srcc) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (WIDGET->serial) free(WIDGET->serial);
  WIDGET->serial=nv;
  WIDGET->serialc=srcc;
  return 0;
}

/* Serialize harmonics as decimal integers 0..999.
 */

static int ps_wave_serialize_harmonics(char *dst,int dsta,const double *coefv,int coefc) {
  if (!dst||(dsta<0)) dsta=0;
  if (coefc<1) return -1;
  while ((coefc>1)&&(coefv[coefc-1]==0.0)) coefc--;
  int dstc=0;
  for (;coefc-->0;coefv++) {
    int coef=(*coefv)*1000;
    if (coef<0) coef=0; else if (coef>999) coef=999;
    dstc+=ps_decsint_repr(dst+dstc,dsta-dstc,coef);
    if (dstc<dsta) dst[dstc]=' ';
    dstc++;
  }
  dstc--;
  return dstc;
}

/* Rebuild serial for IPCM usage.
 */

static int ps_wave_rebuild_serial_ipcm(struct ps_widget *widget) {
  switch (WIDGET->model.preset) {

    case PS_WIDGET_WAVE_PRESET_SINE: return ps_wave_replace_serial(widget,"sine",4);
    case PS_WIDGET_WAVE_PRESET_SQUARE: return ps_wave_replace_serial(widget,"square",6);
    case PS_WIDGET_WAVE_PRESET_SAW: return ps_wave_replace_serial(widget,"saw",3);
    case PS_WIDGET_WAVE_PRESET_NOISE: return ps_wave_replace_serial(widget,"noise",5);
    
    case PS_WIDGET_WAVE_PRESET_HARMONICS: {
        char buf[256];
        int bufc=ps_wave_serialize_harmonics(buf,sizeof(buf),WIDGET->model.coefv,PS_WIDGET_WAVE_COEF_COUNT);
        if ((bufc<0)||(bufc>sizeof(buf))) return -1;
        return ps_wave_replace_serial(widget,buf,bufc);
      }
      
  }
  return -1;
}

/* Rebuild serial for instrument usage.
 */

static int ps_wave_rebuild_serial_instrument(struct ps_widget *widget) {

  /* Compose a complete instrument with 16 coefficients. */
  uint8_t tmp[42];
  tmp[0]=WIDGET->model.attack_time>>8;
  tmp[1]=WIDGET->model.attack_time;
  tmp[2]=WIDGET->model.drawback_time>>8;
  tmp[3]=WIDGET->model.drawback_time;
  tmp[4]=WIDGET->model.decay_time>>8;
  tmp[5]=WIDGET->model.decay_time;
  tmp[6]=WIDGET->model.attack_trim;
  tmp[7]=WIDGET->model.drawback_trim;
  tmp[8]=0;
  tmp[9]=16; // coefc
  int tmpp=10;
  int i=0; for (;i<16;i++,tmpp+=2) {
    int coef=WIDGET->model.coefv[i]*0xffff;
    if (coef<0) coef=0;
    else if (coef>0xffff) coef=0xffff;
    tmp[tmpp]=coef>>8;
    tmp[tmpp+1]=coef;
  }

  /* Trim unused coefficients. */
  int tmpc=42;
  while (tmp[9]>0) {
    if (tmp[tmpc-1]||tmp[tmpc-2]) break; // nonzero, stop here.
    tmpc-=2;
    tmp[9]--;
  }

  return ps_wave_replace_serial(widget,tmp,tmpc);
}

/* Rebuild WIDGET->serial from dynamic UI.
 */

static int ps_wave_rebuild_serial(struct ps_widget *widget) {
  switch (WIDGET->usage) {
    case PS_WIDGET_WAVE_USAGE_IPCM: return ps_wave_rebuild_serial_ipcm(widget);
    case PS_WIDGET_WAVE_USAGE_INSTRUMENT: return ps_wave_rebuild_serial_instrument(widget);
  }
  return -1;
}

/* Decode a preset. Default to HARMONICS.
 */

static int ps_wave_decode_preset(const void *src,int srcc) {
  if ((srcc==4)&&!memcmp(src,"sine",4)) return PS_WIDGET_WAVE_PRESET_SINE;
  if ((srcc==6)&&!memcmp(src,"square",6)) return PS_WIDGET_WAVE_PRESET_SQUARE;
  if ((srcc==3)&&!memcmp(src,"saw",3)) return PS_WIDGET_WAVE_PRESET_SAW;
  if ((srcc==5)&&!memcmp(src,"noise",5)) return PS_WIDGET_WAVE_PRESET_NOISE;
  return PS_WIDGET_WAVE_PRESET_HARMONICS;
}

/* Decode text harmonics into WIDGET->model.coefv.
 * Empty input is equivalent to "999".
 */

static int ps_wave_decode_harmonics(struct ps_widget *widget,const char *src,int srcc) {

  memset(WIDGET->model.coefv,0,sizeof(WIDGET->model.coefv));
  WIDGET->model.coefv[0]=1.0;
  
  int coefp=0,srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    int coef=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
      char ch=src[srcp++];
      if ((ch<'0')||(ch>'9')) return -1;
      coef*=10;
      coef+=ch-'0';
      if (coef>=1000) return -1;
    }
    if (coefp<PS_WIDGET_WAVE_COEF_COUNT) {
      WIDGET->model.coefv[coefp]=coef/1000.0;
    }
    coefp++;
  }

  return 0;
}

/* Rebuild model from serialized IPCM voice.
 * This can be a constant string (sine,square,saw,noise) or whitespace-delimited decimal integers in 0..999.
 */

static int ps_wave_decode_to_model_ipcm(struct ps_widget *widget) {

  int preset=ps_wave_decode_preset(WIDGET->serial,WIDGET->serialc);
  if (preset<0) return -1;
  WIDGET->model.preset=preset;
  if (ps_widget_scrolllist_set_selection(ps_wave_get_presets(widget),WIDGET->model.preset)<0) return -1;

  if (preset==PS_WIDGET_WAVE_PRESET_HARMONICS) {
    if (ps_wave_decode_harmonics(widget,WIDGET->serial,WIDGET->serialc)<0) {
      ps_log(EDIT,ERROR,"Failed to decode wave for ipcm voice.");
      return -1;
    }
  }

  return 0;
}

/* Rebuild model from serialized instrument.
 */

static int ps_wave_decode_to_model_instrument(struct ps_widget *widget) {

  /* Validate incoming data. Length only; beyond that, anything goes. */
  if (WIDGET->serialc<10) {
    ps_log(EDIT,ERROR,"Failed to decode instrument. length=%d",WIDGET->serialc);
    return 0;
  }
  const uint8_t *src=WIDGET->serial;
  int coefc=src[9];
  int reqc=10+(coefc*2);
  if (reqc>WIDGET->serialc) {
    ps_log(EDIT,ERROR,"Short instrument data. length=%d expected=%d",WIDGET->serialc,reqc);
    return 0;
  }

  /* We can safely truncate the coefficients, if they run longer than our fixed buffer.
   * (FWIW, AKAU does this too).
   */
  if (coefc>PS_WIDGET_WAVE_COEF_COUNT) {
    coefc=PS_WIDGET_WAVE_COEF_COUNT;
  }

  /* And we're good. Copy it. */
  memset(&WIDGET->model,0,sizeof(WIDGET->model));
  WIDGET->model.preset=PS_WIDGET_WAVE_PRESET_HARMONICS;
  WIDGET->model.attack_time=(src[0]<<8)|src[1];
  WIDGET->model.drawback_time=(src[2]<<8)|src[3];
  WIDGET->model.decay_time=(src[4]<<8)|src[5];
  WIDGET->model.attack_trim=src[6];
  WIDGET->model.drawback_trim=src[7];

  if (coefc>0) {
    int srcp=10,i=0;
    for (;i<coefc;i++,srcp+=2) {
      int coef=(src[srcp]<<8)|src[srcp+1];
      WIDGET->model.coefv[i]=coef/65535.0;
    }
  } else {
    WIDGET->model.coefv[0]=1.0;
  }
  
  return 0;
}

/* Rebuild model and UI from WIDGET->serial. Dispatch on usage.
 */

static int ps_wave_decode_to_model(struct ps_widget *widget) {
  switch (WIDGET->usage) {
    case PS_WIDGET_WAVE_USAGE_IPCM: {
        if (ps_wave_decode_to_model_ipcm(widget)<0) return -1;
      } break;
    case PS_WIDGET_WAVE_USAGE_INSTRUMENT: {
        if (ps_wave_decode_to_model_instrument(widget)<0) return -1;
      } break;
    default: return -1;
  }
  if (ps_widget_harmonics_model_changed(ps_wave_get_harmonics(widget))<0) return -1;
  if (ps_widget_waveview_model_changed(ps_wave_get_visualization(widget))<0) return -1;
  if (ps_widget_waveenv_model_changed(ps_wave_get_envelope(widget))<0) return -1;
  return 0;
}

/* Accessors.
 */
 
int ps_widget_wave_set_usage(struct ps_widget *widget,int usage) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  if (usage==WIDGET->usage) return 0;
  switch (usage) {
    case PS_WIDGET_WAVE_USAGE_IPCM: return ps_wave_setup_ipcm(widget);
    case PS_WIDGET_WAVE_USAGE_INSTRUMENT: return ps_wave_setup_instrument(widget);
  }
  return -1;
}

int ps_widget_wave_get_usage(const struct ps_widget *widget) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  return WIDGET->usage;
}

int ps_widget_wave_set_serial(struct ps_widget *widget,const void *src,int srcc) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  if (ps_wave_replace_serial(widget,src,srcc)<0) return -1;
  if (ps_wave_decode_to_model(widget)<0) return -1;
  return 0;
}

int ps_widget_wave_serialize(void *dst,int dsta,struct ps_widget *widget) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  if (!dst||(dsta<0)) dsta=0;
  if (ps_wave_rebuild_serial(widget)<0) return -1;
  if (WIDGET->serialc<=dsta) memcpy(dst,WIDGET->serial,WIDGET->serialc);
  return WIDGET->serialc;
}

int ps_widget_wave_set_callback(struct ps_widget *widget,struct ps_callback cb) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  ps_callback_cleanup(&WIDGET->cb);
  WIDGET->cb=cb;
  return 0;
}

int ps_widget_wave_set_refnum1(struct ps_widget *widget,int v) {
  if (ps_wave_obj_validate(widget)<0) return -1;
  WIDGET->refnum1=v;
  return 0;
}

int ps_widget_wave_get_refnum1(const struct ps_widget *widget) {
  if (ps_wave_obj_validate(widget)<0) return 0;
  return WIDGET->refnum1;
}

/* Button callbacks.
 */
 
static int ps_wave_cb_ok(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_callback_call(&WIDGET->cb,widget)<0) return -1;
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_wave_cb_cancel(struct ps_widget *button,struct ps_widget *widget) {
  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

static int ps_wave_cb_preset(struct ps_widget *scrolllist,struct ps_widget *widget) {

  int selection=ps_widget_scrolllist_get_selection(scrolllist);
  if (selection==WIDGET->model.preset) return 0;
  if (selection<0) return 0;
  if (WIDGET->usage!=PS_WIDGET_WAVE_USAGE_IPCM) return -1;
  switch (selection) {
    case PS_WIDGET_WAVE_PRESET_SINE:
    case PS_WIDGET_WAVE_PRESET_SQUARE:
    case PS_WIDGET_WAVE_PRESET_SAW:
    case PS_WIDGET_WAVE_PRESET_NOISE:
    case PS_WIDGET_WAVE_PRESET_HARMONICS:
      break;
    default: return -1;
  }
  
  WIDGET->model.preset=selection;

  if (ps_widget_wave_dirty(widget)<0) return -1;
  
  return 0;
}

/* Get model.
 */
 
struct ps_wave_ui_model *ps_widget_wave_get_model(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_wave)) widget=widget->parent;
  if (ps_wave_obj_validate(widget)<0) return 0;
  return &WIDGET->model;
}

/* Dirty event.
 */

int ps_widget_wave_dirty(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_wave)) widget=widget->parent;
  if (ps_wave_obj_validate(widget)<0) return -1;

  /* Presets already take care of themselves, through the scrolllist callback.
   * Harmonics rereads the model on every operation.
   * Waveview needs alerted.
   */
   
  akau_fpcm_del(WIDGET->fpcm);
  WIDGET->fpcm=0;

  akau_instrument_del(WIDGET->instrument);
  WIDGET->instrument=0;

  if (ps_widget_waveview_model_changed(ps_wave_get_visualization(widget))<0) return -1;
  if (ps_widget_waveenv_model_changed(ps_wave_get_envelope(widget))<0) return -1;
   
  return 0;
}

/* Get fpcm, generate if needed.
 */

static struct akau_fpcm *ps_wave_rebuild_fpcm(const struct ps_wave_ui_model *model) {
  switch (model->preset) {
    case PS_WIDGET_WAVE_PRESET_SINE: return akau_generate_fpcm_sine(44100,0);
    case PS_WIDGET_WAVE_PRESET_SQUARE: return akau_generate_fpcm_square(44100,0);
    case PS_WIDGET_WAVE_PRESET_SAW: return akau_generate_fpcm_saw(44100);
    case PS_WIDGET_WAVE_PRESET_NOISE: return akau_generate_fpcm_whitenoise(44100);
    case PS_WIDGET_WAVE_PRESET_HARMONICS: return akau_generate_fpcm_harmonics(0,model->coefv,PS_WIDGET_WAVE_COEF_COUNT,1);
  }
  return 0;
}
 
struct akau_fpcm *ps_widget_wave_get_fpcm(struct ps_widget *widget) {
  while (widget&&(widget->type!=&ps_widget_type_wave)) widget=widget->parent;
  if (ps_wave_obj_validate(widget)<0) return 0;
  if (!WIDGET->fpcm) {
    WIDGET->fpcm=ps_wave_rebuild_fpcm(&WIDGET->model);
    if (akau_fpcm_amplify(WIDGET->fpcm,1.0/akau_fpcm_calculate_peak(WIDGET->fpcm))<0) return 0;
  }
  return WIDGET->fpcm;
}

/* Get instrument, generate if needed.
 */
 
struct akau_instrument *ps_widget_wave_get_instrument(struct ps_widget *widget) {
  struct akau_fpcm *fpcm=ps_widget_wave_get_fpcm(widget);
  if (!fpcm) return 0;
  if (!WIDGET->instrument) {
    const int frames_per_millisecond=44;
    WIDGET->instrument=akau_instrument_new(
      WIDGET->fpcm,
      WIDGET->model.attack_time*frames_per_millisecond,
      WIDGET->model.attack_trim/255.0,
      WIDGET->model.drawback_time*frames_per_millisecond,
      WIDGET->model.drawback_trim/255.0,
      WIDGET->model.decay_time*frames_per_millisecond
    );
  }
  return WIDGET->instrument;
}
