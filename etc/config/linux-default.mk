CCWARN:=-Werror -Wimplicit -Wno-overflow
CCINC:=-Isrc -I$(MIDDIR)

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN)
OBJC:=
LD:=gcc 
LDPOST:=-lz -lm -lasound -lpthread -lX11 -lGL

OPT_ENABLE:=glx alsa evdev genioc

PS_GLSL_VERSION:=120

RELEASE_FILES:=$(subst $(OUTDIR)/,,$(EXE_MAIN) $(DATA_ARCHIVE) $(INPUTCFG) $(MAINCFG))

platform-release:; \
  cd $(OUTDIR) ; \
  rm -f plundersquad*.zip ; \
  VERSION=$$(git tag -l --points-at HEAD) ; \
  if [ -z "$$VERSION" ] ; then VERSION=$$(date +%Y%m%d-%H%M) ; fi ; \
  zip -qr plundersquad-linux-$$VERSION.zip $(RELEASE_FILES) || exit 1 ; \
  echo "Built release package $(OUTDIR)/plundersquad-linux-$$VERSION.zip"
