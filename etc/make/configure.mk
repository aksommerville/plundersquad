ifndef PS_CONFIG
  UNAMES:=$(shell uname -s)
  ifeq ($(UNAMES),Darwin)
    PS_ARCH:=macos
  else ifeq ($(UNAMES),Linux)
    UNAMEN:=$(shell uname -n)
    ifeq ($(UNAMEN),raspberrypi)
      #PS_ARCH:=raspi
      PS_ARCH:=linux
      PS_CONFIG:=linux-drm
    else ifeq ($(UNAMEN),vcs)
      PS_ARCH:=linux
      PS_CONFIG:=linux-drm
    else
      PS_ARCH:=linux
    endif
  else ifneq (,$(strip $(filter MINGW%,$(UNAMES))))
    PS_ARCH:=mswin
  else
    $(error Unable to guess host architecture. Please update etc/configure.mk or set PS_CONFIG.)
  endif
  ifndef PS_CONFIG
    PS_CONFIG:=$(PS_ARCH)-default
  endif
else
  PS_ARCH:=$(firstword $(subst -, ,$(PS_CONFIG)))
endif

export PS_CONFIG
export PS_ARCH

MIDDIR:=mid/$(PS_CONFIG)
OUTDIR:=out/$(PS_CONFIG)

# These may be overridden by configuration:
EXE_MAIN:=$(OUTDIR)/plundersquad
EXE_TEST:=$(OUTDIR)/test
EXE_EDIT:=$(OUTDIR)/plundersquad-editor
EXE_RESPACK:=$(OUTDIR)/respack
CMD_MAIN=$(EXE_MAIN)
CMD_TEST=$(EXE_TEST)
CMD_EDIT=$(EXE_EDIT) --resources=src/data
CMD_RESPACK=$(EXE_RESPACK)
DATA_ARCHIVE:=$(OUTDIR)/ps-data
INPUTCFG:=$(OUTDIR)/input.cfg
MAINCFG:=$(OUTDIR)/plundersquad.cfg
export COMPILE_TIME_CONSTANTS:=PS_GLSL_VERSION
export PS_GLSL_VERSION:=120

# These must be set by configuration (OBJC only for MacOS):
CC:=
OBJC:=
AR:=
LD:=
LDPOST:=
export OPT_ENABLE:=

include etc/config/$(PS_CONFIG).mk
