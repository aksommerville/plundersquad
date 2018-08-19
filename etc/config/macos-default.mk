CCWARN:=-Werror -Wimplicit -Wno-parentheses -Wno-constant-conversion -Wno-deprecated-declarations -Wno-comment -Wno-pointer-sign
CCINC:=-Isrc -I$(MIDDIR)

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN)
OBJC:=gcc -xobjective-c -c -MMD -O2 $(CCINC) $(CCWARN)
LD:=gcc
LDPOST:=-framework Cocoa -framework OpenGL -framework AudioUnit -lz -framework IOKit

OPT_ENABLE:=macioc macwm machid akmacaudio

PS_GLSL_VERSION:=120

BUNDLE_MAIN:=$(OUTDIR)/PlunderSquad.app
PLIST_MAIN:=$(BUNDLE_MAIN)/Contents/Info.plist
NIB_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/Main.nib
EXE_MAIN:=$(BUNDLE_MAIN)/Contents/MacOS/PlunderSquad
DATA_ARCHIVE:=$(BUNDLE_MAIN)/Contents/Resources/ps-data
INPUTCFG_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/input.cfg
CFG_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/plundersquad.cfg
ICON_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/appicon.icns

BUNDLE_EDIT:=$(OUTDIR)/EditPlunderSquad.app
PLIST_EDIT:=$(BUNDLE_EDIT)/Contents/Info.plist
NIB_EDIT:=$(BUNDLE_EDIT)/Contents/Resources/Main.nib
EXE_EDIT:=$(BUNDLE_EDIT)/Contents/MacOS/EditPlunderSquad
CFG_EDIT:=$(BUNDLE_EDIT)/Contents/Resources/plundersquad.cfg

$(EXE_MAIN):$(PLIST_MAIN) $(NIB_MAIN) $(CFG_MAIN) $(INPUTCFG_MAIN) $(ICON_MAIN)
$(EXE_EDIT):$(PLIST_EDIT) $(NIB_EDIT) $(CFG_EDIT)

$(PLIST_MAIN):src/main/Info.plist;$(PRECMD) cp $< $@
$(NIB_MAIN):src/main/Main.xib;$(PRECMD) ibtool --compile $@ $<

$(PLIST_EDIT):src/edit/Info.plist;$(PRECMD) cp $< $@
$(NIB_EDIT):src/edit/Main.xib;$(PRECMD) ibtool --compile $@ $<

$(CFG_EDIT):etc/plundersquad.cfg;$(PRECMD) cp $< $@
INPUTCFG:=$(INPUTCFG_MAIN)
MAINCFG:=$(CFG_MAIN)

CMD_MAIN:=open -W $(BUNDLE_MAIN) --args --reopen-tty=$$(tty) --chdir=$$(pwd)
CMD_EDIT:=open -W $(BUNDLE_EDIT) --args --reopen-tty=$$(tty) --chdir=$$(pwd)

clean:remove-macos-preferences
remove-macos-preferences:;rm -rf ~/Library/Preferences/com.aksommerville.plundersquad

INPUT_ICONS:=$(wildcard src/main/appicon.iconset/*)
$(ICON_MAIN):$(INPUT_ICONS);$(PRECMD) iconutil -c icns -o $@ src/main/appicon.iconset

sign-app:all;codesign -s '3rd Party Mac Developer Application: Andy Sommerville (4G7NZRMQYQ)' $(BUNDLE_MAIN)

platform-release:sign-app; \
  cd $(OUTDIR) ; \
  rm -f plundersquad*.zip ; \
  VERSION=$$(git tag -l --points-at HEAD) ; \
  if [ -z "$$VERSION" ] ; then VERSION=$$(date +%Y%m%d-%H%M) ; fi ; \
  zip -qr plundersquad-macos-$$VERSION.zip PlunderSquad.app || exit 1 ; \
  echo "Built release package $(OUTDIR)/plundersquad-macos-$$VERSION.zip"
