GENERATED_FILES:=$(addprefix $(MIDDIR)/, \
  ps_config.h \
  ps_test_contents.h \
  video/ps_appicon.c \
)

$(MIDDIR)/ps_config.h:etc/tool/mkconfig.sh etc/config/$(PS_CONFIG).mk;$(PRECMD) $< $@
$(MIDDIR)/ps_test_contents.h:etc/tool/mktest.sh $(shell find src/test -name '*.c');$(PRECMD) $^ $@

# MinGW doesn't provide hexdump. Grrr
# Writing my own version of it in Python because that's the kind of weirdo I am.
ifeq ($(PS_ARCH),mswin)
  $(MIDDIR)/video/ps_appicon.c:etc/tool/mkappicon.sh src/video/appicon.*.rgba;$(PRECMD) PATH=$$PATH:etc/tool $^ $@
else
  $(MIDDIR)/video/ps_appicon.c:etc/tool/mkappicon.sh src/video/appicon.*.rgba;$(PRECMD) $^ $@
endif
