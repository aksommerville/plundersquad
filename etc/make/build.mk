OPT_AVAILABLE:=$(notdir $(wildcard src/opt/*))
OPT_DISABLE:=$(filter-out $(OPT_ENABLE),$(OPT_AVAILABLE))
OPT_ERROR:=$(filter-out $(OPT_AVAILABLE),$(OPT_ENABLE))
ifneq (,$(strip $(OPT_ERROR)))
  $(warning Optional units requested but not available: $(OPT_ERROR))
endif
export OPT_ENABLE
export OPT_DISABLE

OPT_IGNORE_PATTERN_BASE:=$(addsuffix /%,$(OPT_DISABLE))
OPT_IGNORE_PATTERN:=$(foreach SFX,$(OPT_IGNORE_PATTERN_BASE), \
  src/opt/$(SFX) \
  src/test/opt/$(SFX) \
  $(MIDDIR)/opt/$(SFX) \
  $(MIDDIR)/test/opt/$(SFX) \
)

GENERATED_FILES:=$(filter-out $(OPT_IGNORE_PATTERN),$(GENERATED_FILES))
GENHFILES:=$(filter %.h,$(GENERATED_FILES))

SRCFILES:=$(filter-out $(OPT_IGNORE_PATTERN),$(shell find src -type f)) $(GENERATED_FILES)
SRCFILES_C:=$(filter %.c,$(SRCFILES))
SRCFILES_M:=$(filter %.m,$(SRCFILES))

OFILES_ALL:=$(addsuffix .o,$(patsubst src/%,$(MIDDIR)/%,$(basename $(SRCFILES_C) $(SRCFILES_M))))
-include $(OFILES_ALL:.o=.d)

OFILES_MAIN:=$(filter $(MIDDIR)/main/%,$(OFILES_ALL))
OFILES_TEST:=$(filter $(MIDDIR)/test/%,$(OFILES_ALL))
OFILES_EDIT:=$(filter $(MIDDIR)/edit/%,$(OFILES_ALL))
OFILES_RESPACK:=$(filter $(MIDDIR)/respack/%,$(OFILES_ALL))
OFILES_COMMON:=$(filter-out $(OFILES_MAIN) $(OFILES_TEST) $(OFILES_EDIT) $(OFILES_RESPACK),$(OFILES_ALL))
OFILES_MAIN:=$(OFILES_COMMON) $(OFILES_MAIN)
OFILES_TEST:=$(OFILES_COMMON) $(OFILES_TEST)
OFILES_EDIT:=$(OFILES_COMMON) $(OFILES_EDIT)
OFILES_RESPACK:=$(OFILES_COMMON) $(OFILES_RESPACK)

# Some extra rules to reduce the size of the main executable.
OFILES_MAIN:=$(filter-out $(MIDDIR)/gui/editor/%,$(OFILES_MAIN))

ifneq (,$(strip $(SRCFILES_M)))
  ifeq (,$(strip $(OBJC)))
    $(error Objective-C compiler required but not defined. ($(SRCFILES_M)))
  endif
endif

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<

$(MIDDIR)/%.o:src/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<

# !!! Linker commands are failing on Windows, I think because the commands are too long.
# For crying out loud.
# Doing it this way, builds take for-goddamn-ever.
# You can disable this for an intermediate build, but it won't link new files.
ifeq ($(PS_ARCH),mswin)
  OFILES_LIST_MAIN:=$(MIDDIR)/ofiles-list-main
  OFILES_LIST_TEST:=$(MIDDIR)/ofiles-list-test
  OFILES_LIST_EDIT:=$(MIDDIR)/ofiles-list-edit
  OFILES_LIST_RESPACK:=$(MIDDIR)/ofiles-list-respack
  ifeq (rebuild file lists,rebuild file lists)
    OFILES_LIST:=$(MIDDIR)/ofiles-list
    $(shell rm -f $(OFILES_LIST))
    $(foreach F,$(OFILES_ALL),$(shell echo $F >>$(OFILES_LIST)))
    $(shell sed -E '/$(PS_CONFIG)\/(test|edit|respack)\//d' $(OFILES_LIST) > $(OFILES_LIST_MAIN))
    $(shell sed -E '/$(PS_CONFIG)\/(main|edit|respack)\//d' $(OFILES_LIST) > $(OFILES_LIST_TEST))
    $(shell sed -E '/$(PS_CONFIG)\/(test|main|respack)\//d' $(OFILES_LIST) > $(OFILES_LIST_EDIT))
    $(shell sed -E '/$(PS_CONFIG)\/(test|edit|main)\//d' $(OFILES_LIST) > $(OFILES_LIST_RESPACK))
  endif
  $(EXE_MAIN):$(OFILES_MAIN) $(DATA_ARCHIVE);$(PRECMD) $(LD) -o $@ @$(OFILES_LIST_MAIN) $(LDPOST)
  $(EXE_TEST):$(OFILES_TEST);$(PRECMD) $(LD) -o $@ @$(OFILES_LIST_TEST) $(LDPOST)
  $(EXE_EDIT):$(OFILES_EDIT);$(PRECMD) $(LD) -o $@ @$(OFILES_LIST_EDIT) $(LDPOST)
  $(EXE_RESPACK):$(OFILES_RESPACK);$(PRECMD) $(LD) -o $@ @$(OFILES_LIST_RESPACK) $(LDPOST)

# Meanwhile, in the civilized world:
else
  $(EXE_MAIN):$(OFILES_MAIN) $(DATA_ARCHIVE);$(PRECMD) $(LD) -o $@ $(OFILES_MAIN) $(LDPOST)
  $(EXE_TEST):$(OFILES_TEST);$(PRECMD) $(LD) -o $@ $(OFILES_TEST) $(LDPOST)
  $(EXE_EDIT):$(OFILES_EDIT);$(PRECMD) $(LD) -o $@ $(OFILES_EDIT) $(LDPOST)
  $(EXE_RESPACK):$(OFILES_RESPACK);$(PRECMD) $(LD) -o $@ $(OFILES_RESPACK) $(LDPOST)
endif

all:$(EXE_MAIN) $(EXE_TEST) $(EXE_EDIT) $(EXE_RESPACK) $(DATA_ARCHIVE)

DATA_SRC_FILES:=$(shell find src/data -type f)
$(DATA_ARCHIVE):$(DATA_SRC_FILES) $(EXE_RESPACK);$(PRECMD) $(CMD_RESPACK) $@ src/data
