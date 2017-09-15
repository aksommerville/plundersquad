CCWARN:=-Werror -Wimplicit -Wno-parentheses -Wno-constant-conversion -Wno-deprecated-declarations
CCINC:=-Isrc -I$(MIDDIR)

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN)
OBJC:=gcc -xobjective-c -c -MMD -O2 $(CCINC) $(CCWARN)
LD:=gcc
LDPOST:=-framework Cocoa -framework OpenGL -framework AudioUnit -lz -framework IOKit

OPT_ENABLE:=macioc macwm

PS_GLSL_VERSION:=120

BUNDLE_MAIN:=$(OUTDIR)/PlunderSquad.app
PLIST_MAIN:=$(BUNDLE_MAIN)/Contents/Info.plist
NIB_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/Main.nib
EXE_MAIN:=$(BUNDLE_MAIN)/Contents/MacOS/PlunderSquad
OUTDIR_MAIN:=$(BUNDLE_MAIN)/Contents/Resources

BUNDLE_EDIT:=$(OUTDIR)/EditPlunderSquad.app
PLIST_EDIT:=$(BUNDLE_EDIT)/Contents/Info.plist
NIB_EDIT:=$(BUNDLE_EDIT)/Contents/Resources/Main.nib
EXE_EDIT:=$(BUNDLE_EDIT)/Contents/MacOS/EditPlunderSquad

$(EXE_MAIN):$(PLIST_MAIN) $(NIB_MAIN)
$(EXE_EDIT):$(PLIST_EDIT) $(NIB_EDIT)

$(PLIST_MAIN):src/main/Info.plist;$(PRECMD) cp $< $@
$(NIB_MAIN):src/main/Main.xib;$(PRECMD) ibtool --compile $@ $<

$(PLIST_EDIT):src/edit/Info.plist;$(PRECMD) cp $< $@
$(NIB_EDIT):src/edit/Main.xib;$(PRECMD) ibtool --compile $@ $<

CMD_MAIN:=open -W $(BUNDLE_MAIN) --args --reopen-tty=$$(tty) --chdir=$$(pwd)
CMD_EDIT:=open -W $(BUNDLE_EDIT) --args --reopen-tty=$$(tty) --chdir=$$(pwd)
