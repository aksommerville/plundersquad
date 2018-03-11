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

ifneq (,$(strip $(SRCFILES_M)))
  ifeq (,$(strip $(OBJC)))
    $(error Objective-C compiler required but not defined. ($(SRCFILES_M)))
  endif
endif

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<

$(MIDDIR)/%.o:src/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<

all:$(EXE_MAIN) $(EXE_TEST) $(EXE_EDIT) $(EXE_RESPACK) $(DATA_ARCHIVE)
$(EXE_MAIN):$(OFILES_MAIN) $(DATA_ARCHIVE);$(PRECMD) $(LD) -o $@ $(OFILES_MAIN) $(LDPOST)
$(EXE_TEST):$(OFILES_TEST);$(PRECMD) $(LD) -o $@ $(OFILES_TEST) $(LDPOST)
$(EXE_EDIT):$(OFILES_EDIT);$(PRECMD) $(LD) -o $@ $(OFILES_EDIT) $(LDPOST)
$(EXE_RESPACK):$(OFILES_RESPACK);$(PRECMD) $(LD) -o $@ $(OFILES_RESPACK) $(LDPOST)

DATA_SRC_FILES:=$(shell find src/data -type f)
$(DATA_ARCHIVE):$(DATA_SRC_FILES) $(EXE_RESPACK);$(PRECMD) $(CMD_RESPACK) $@ src/data
