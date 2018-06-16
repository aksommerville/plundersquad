CCWARN:=-Werror -Wimplicit -Wno-overflow
CCINC:=-Isrc -I$(MIDDIR)
WINDEF:=-m32 -mwindows -D_WIN32_WINNT=0x0501 -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -DPS_NO_OPENGL2=1 -D_PTW32_STATIC_LIB=1

CC:=gcc -c -MMD -O2 $(CCINC) $(CCWARN) $(WINDEF)
OBJC:=
AR:=ar rc
LD:=gcc -m32 -mwindows -Wl,-static
LDPOST:=-lz -lglew32s -lopengl32 -lwinmm -lpthread -lhid

OPT_ENABLE:=mswm msaudio mshid genioc

PS_GLSL_VERSION:=120

EXE_MAIN:=$(OUTDIR)/plundersquad.exe
EXE_TEST:=$(OUTDIR)/test.exe
EXE_EDIT:=$(OUTDIR)/edit.exe
EXE_RESPACK:=$(OUTDIR)/respack.exe

# Wasn't a problem on my old Windows box, but the new one, in msys terminal, doesn't print stderr or stdout from app.
# However if we redirect that output and explicitly 'echo' it, there it is.
# For fuck's sake.
RIDICULOUS_WORKAROUND_FOR_MSYS_CONSOLE:=2>&1 | while read LINE ; do echo "$$LINE" ; done
POST_CMD_TEST:=$(RIDICULOUS_WORKAROUND_FOR_MSYS_CONSOLE)
POST_CMD_MAIN:=$(RIDICULOUS_WORKAROUND_FOR_MSYS_CONSOLE)
