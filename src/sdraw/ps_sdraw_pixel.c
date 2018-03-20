#include "ps.h"
#include "ps_sdraw.h"

/* Pixel format properties.
 */
 
int ps_sdraw_fmt_has_rgb(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return 1;
    case PS_SDRAW_FMT_RGBX: return 1;
    case PS_SDRAW_FMT_RGBA: return 1;
    case PS_SDRAW_FMT_A: return 0;
  }
  return 0;
}

int ps_sdraw_fmt_has_alpha(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return 0;
    case PS_SDRAW_FMT_RGBX: return 0;
    case PS_SDRAW_FMT_RGBA: return 1;
    case PS_SDRAW_FMT_A: return 1;
  }
  return 0;
}

int ps_sdraw_fmt_has_luma(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return 0;
    case PS_SDRAW_FMT_RGBX: return 0;
    case PS_SDRAW_FMT_RGBA: return 0;
    case PS_SDRAW_FMT_A: return 0;
  }
  return 0;
}

int ps_sdraw_fmt_pixel_size(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return 3;
    case PS_SDRAW_FMT_RGBX: return 4;
    case PS_SDRAW_FMT_RGBA: return 4;
    case PS_SDRAW_FMT_A: return 1;
  }
  return 0;
}

/* Pixel accessor functions.
 */

static struct ps_sdraw_rgba ps_sdraw_pxrd_RGB(const uint8_t *src) {
  return ps_sdraw_rgba(src[0],src[1],src[2],0xff);
}

static void ps_sdraw_pxwr_RGB(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  if (!rgba.a) return;
  if (rgba.a==0xff) {
    dst[0]=rgba.r;
    dst[1]=rgba.g;
    dst[2]=rgba.b;
  } else {
    uint8_t dsta=0xff-rgba.a;
    dst[0]=(dst[0]*dsta+rgba.r*rgba.a)>>8;
    dst[1]=(dst[1]*dsta+rgba.g*rgba.a)>>8;
    dst[2]=(dst[2]*dsta+rgba.b*rgba.a)>>8;
  }
}

static void ps_sdraw_pxcp_RGB(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  dst[0]=rgba.r;
  dst[1]=rgba.g;
  dst[2]=rgba.b;
}

static void ps_sdraw_pxwr_RGBX(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  if (!rgba.a) return;
  if (rgba.a==0xff) {
    dst[0]=rgba.r;
    dst[1]=rgba.g;
    dst[2]=rgba.b;
    dst[3]=0xff;
  } else {
    uint8_t dsta=0xff-rgba.a;
    dst[0]=(dst[0]*dsta+rgba.r*rgba.a)>>8;
    dst[1]=(dst[1]*dsta+rgba.g*rgba.a)>>8;
    dst[2]=(dst[2]*dsta+rgba.b*rgba.a)>>8;
    dst[3]=0xff;
  }
}

static void ps_sdraw_pxcp_RGBX(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  dst[0]=rgba.r;
  dst[1]=rgba.g;
  dst[2]=rgba.b;
  dst[3]=0xff;
}

static struct ps_sdraw_rgba ps_sdraw_pxrd_RGBA(const uint8_t *src) {
  return ps_sdraw_rgba(src[0],src[1],src[2],src[3]);
}

static void ps_sdraw_pxwr_RGBA(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  if (rgba.a==0) return;
  if ((rgba.a==0xff)||!dst[3]) {
    dst[0]=rgba.r;
    dst[1]=rgba.g;
    dst[2]=rgba.b;
    dst[3]=rgba.a;
  } else {
    int a=dst[3]+rgba.a;
    int r=(dst[0]*dst[3]+rgba.r*rgba.a)/a;
    int g=(dst[1]*dst[3]+rgba.g*rgba.a)/a;
    int b=(dst[2]*dst[3]+rgba.b*rgba.a)/a;
    dst[0]=r;
    dst[1]=g;
    dst[2]=b;
    dst[3]=(a>=255)?255:a;
  }
}

static void ps_sdraw_pxcp_RGBA(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  dst[0]=rgba.r;
  dst[1]=rgba.g;
  dst[2]=rgba.b;
  dst[3]=rgba.a;
}

static struct ps_sdraw_rgba ps_sdraw_pxrd_A(const uint8_t *src) {
  return ps_sdraw_rgba(0,0,0,src[0]);
}

static void ps_sdraw_pxwr_A(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  int a=dst[0]+rgba.a;
  if (a>=0xff) dst[0]=0xff;
  else dst[0]=a;
}

static void ps_sdraw_pxcp_A(uint8_t *dst,struct ps_sdraw_rgba rgba) {
  dst[0]=rgba.a;
}
 
ps_sdraw_pxrd_fn ps_sdraw_pxrd_for_fmt(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return ps_sdraw_pxrd_RGB;
    case PS_SDRAW_FMT_RGBX: return ps_sdraw_pxrd_RGB; // sic RGB
    case PS_SDRAW_FMT_RGBA: return ps_sdraw_pxrd_RGBA;
    case PS_SDRAW_FMT_A: return ps_sdraw_pxrd_A;
  }
  return 0;
}

ps_sdraw_pxwr_fn ps_sdraw_pxwr_for_fmt(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return ps_sdraw_pxwr_RGB;
    case PS_SDRAW_FMT_RGBX: return ps_sdraw_pxwr_RGBX;
    case PS_SDRAW_FMT_RGBA: return ps_sdraw_pxwr_RGBA;
    case PS_SDRAW_FMT_A: return ps_sdraw_pxwr_A;
  }
  return 0;
}

ps_sdraw_pxwr_fn ps_sdraw_pxcp_for_fmt(int fmt) {
  switch (fmt) {
    case PS_SDRAW_FMT_RGB: return ps_sdraw_pxcp_RGB;
    case PS_SDRAW_FMT_RGBX: return ps_sdraw_pxcp_RGBX;
    case PS_SDRAW_FMT_RGBA: return ps_sdraw_pxcp_RGBA;
    case PS_SDRAW_FMT_A: return ps_sdraw_pxcp_A;
  }
  return 0;
}
