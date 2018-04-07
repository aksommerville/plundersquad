/* ps_widget_audiocfgpage.c
 * Modal screen for setting audio channel levels.
 */

#include "ps.h"
#include "../ps_widget.h"
#include "gui/menus/ps_menus.h"
#include "gui/corewidgets/ps_corewidgets.h"
#include "os/ps_userconfig.h"
#include "akau/akau.h"

static int ps_audiocfgpage_cb_menu(struct ps_widget *menu,struct ps_widget *widget);
static int ps_audiocfgpage_cb_bgm(struct ps_widget *slider,struct ps_widget *widget);
static int ps_audiocfgpage_cb_sfx(struct ps_widget *slider,struct ps_widget *widget);

/* Object definition.
 */

struct ps_widget_audiocfgpage {
  struct ps_widget hdr;
};

#define WIDGET ((struct ps_widget_audiocfgpage*)widget)

/* Delete.
 */

static void _ps_audiocfgpage_del(struct ps_widget *widget) {
}

/* Initialize.
 */

static int _ps_audiocfgpage_init(struct ps_widget *widget) {

  widget->bgrgba=0x004080c0;

  struct ps_widget *menu=ps_widget_spawn(widget,&ps_widget_type_menu);
  struct ps_widget *menupacker=ps_widget_menu_get_packer(menu);
  if (!menupacker) return -1;
  if (ps_widget_menu_set_callback(menu,ps_callback(ps_audiocfgpage_cb_menu,0,widget))<0) return -1;

  struct ps_widget *thumb=ps_widget_menu_get_thumb(menu);
  if (!thumb) return -1;
  thumb->bgrgba=0x102040ff;

  struct ps_widget *child;
  const uint32_t textcolor=    0xffffffff;
  const uint32_t sliderbgcolor=0x808080c0;
  const uint32_t sliderfgcolor=0xffff00ff;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=sliderbgcolor;
  child->fgrgba=textcolor;
  if (ps_widget_slider_set_variable_color(child,sliderfgcolor,sliderfgcolor)<0) return -1;
  if (ps_widget_slider_set_text(child,"Music",-1)<0) return -1;
  if (ps_widget_slider_set_text_color(child,textcolor)<0) return -1;
  if (ps_widget_slider_set_limits(child,0,255)<0) return -1;
  if (ps_widget_slider_set_value(child,akau_get_trim_for_intent(AKAU_INTENT_BGM))<0) return -1;
  if (ps_widget_slider_set_increment(child,16)<0) return -1;
  if (ps_widget_slider_set_callback(child,ps_callback(ps_audiocfgpage_cb_bgm,0,widget))<0) return -1;

  if (!(child=ps_widget_spawn(menupacker,&ps_widget_type_slider))) return -1;
  child->bgrgba=sliderbgcolor;
  child->fgrgba=textcolor;
  if (ps_widget_slider_set_variable_color(child,sliderfgcolor,sliderfgcolor)<0) return -1;
  if (ps_widget_slider_set_text(child,"Sound Effects",-1)<0) return -1;
  if (ps_widget_slider_set_text_color(child,textcolor)<0) return -1;
  if (ps_widget_slider_set_limits(child,0,255)<0) return -1;
  if (ps_widget_slider_set_value(child,akau_get_trim_for_intent(AKAU_INTENT_SFX))<0) return -1;
  if (ps_widget_slider_set_increment(child,16)<0) return -1;
  if (ps_widget_slider_set_callback(child,ps_callback(ps_audiocfgpage_cb_sfx,0,widget))<0) return -1;
  
  if (!(child=ps_widget_menu_spawn_label(menu,"Done",4))) return -1;
  child->fgrgba=textcolor;
  
  return 0;
}

/* Pack.
 */

static int _ps_audiocfgpage_pack(struct ps_widget *widget) {
  if (widget->childc!=1) return -1;
  struct ps_widget *menu=widget->childv[0];
  int chw,chh;
  if (ps_widget_measure(&chw,&chh,menu,widget->w,widget->h)<0) return -1;
  menu->x=(widget->w>>1)-(chw>>1);
  menu->y=(widget->h>>1)-(chh>>1);
  menu->w=chw;
  menu->h=chh;
  if (ps_widget_pack(menu)<0) return -1;
  return 0;
}

/* Input.
 */

static int _ps_audiocfgpage_userinput(struct ps_widget *widget,int plrid,int btnid,int value) {
  if (widget->childc!=1) return -1;
  return ps_widget_userinput(widget->childv[0],plrid,btnid,value);
}

/* Type definition.
 */

const struct ps_widget_type ps_widget_type_audiocfgpage={

  .name="audiocfgpage",
  .objlen=sizeof(struct ps_widget_audiocfgpage),

  .del=_ps_audiocfgpage_del,
  .init=_ps_audiocfgpage_init,

  .pack=_ps_audiocfgpage_pack,

  .userinput=_ps_audiocfgpage_userinput,

};

/* Menu item: Done
 */

static int ps_audiocfgpage_menucb_done(struct ps_widget *widget) {

  int bgm=akau_get_trim_for_intent(AKAU_INTENT_BGM);
  int sfx=akau_get_trim_for_intent(AKAU_INTENT_SFX);
  if ((bgm>=0)&&(sfx>=0)) {
    struct ps_userconfig *userconfig=ps_widget_get_userconfig(widget);
    if (userconfig) {
      if (ps_userconfig_set_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"music",5),bgm)>=0) {
      }
      if (ps_userconfig_set_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"sound",5),sfx)>=0) {
      }
    }
  }

  if (ps_widget_kill(widget)<0) return -1;
  return 0;
}

/* Menu callback.
 */
 
static int ps_audiocfgpage_cb_menu(struct ps_widget *menu,struct ps_widget *widget) {
  int p=ps_widget_menu_get_selected_index(menu);
  switch (p) {
    case 2: return ps_audiocfgpage_menucb_done(widget);
  }
  return 0;
}

/* Slider callbacks.
 */
 
static int ps_audiocfgpage_cb_bgm(struct ps_widget *slider,struct ps_widget *widget) {
  int trim=ps_widget_slider_get_value(slider);
  if (trim<0) trim=0; else if (trim>255) trim=255;
  return akau_set_trim_for_intent(AKAU_INTENT_BGM,trim);
}

static int ps_audiocfgpage_cb_sfx(struct ps_widget *slider,struct ps_widget *widget) {
  int trim=ps_widget_slider_get_value(slider);
  if (trim<0) trim=0; else if (trim>255) trim=255;
  return akau_set_trim_for_intent(AKAU_INTENT_SFX,trim);
}
