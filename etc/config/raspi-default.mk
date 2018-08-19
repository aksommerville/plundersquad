CCWARN:=-Werror -Wimplicit -Wno-overflow
CCRPI:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
CCINC:=-Isrc -I$(MIDDIR)
LDRPI:=-L/opt/vc/lib -lGLESv2 -lEGL -lbcm_host

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN) $(CCRPI)
OBJC:=
LD:=gcc $(LDRPI)
LDPOST:=-lz -lm -lasound -lpthread

OPT_ENABLE:=bcm alsa evdev genioc

PS_GLSL_VERSION:=100

# Capture logs to a regular file because the default Pi console doesn't afford much scrollback.
CMD_MAIN:=$(EXE_MAIN) 2>&1 | tee ps.log

RELEASE_FILES:=$(subst $(OUTDIR)/,,$(EXE_MAIN) $(DATA_ARCHIVE) $(INPUTCFG) $(MAINCFG))

platform-release:; \
  cd $(OUTDIR) ; \
  rm -f plundersquad*.zip ; \
  VERSION=$$(git tag -l --points-at HEAD) ; \
  if [ -z "$$VERSION" ] ; then VERSION=$$(date +%Y%m%d-%H%M) ; fi ; \
  zip -qr plundersquad-raspi-$$VERSION.zip $(RELEASE_FILES) || exit 1 ; \
  echo "Built release package $(OUTDIR)/plundersquad-raspi-$$VERSION.zip"
