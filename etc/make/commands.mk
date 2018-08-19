run:$(EXE_MAIN);$(CMD_MAIN) $(POST_CMD_MAIN)

# This $(subst) is necessary because we can't take '=' through MAKECMDGOALS.
# Maybe that's just a Mac thing, I'd never heard of it before.
# This is for developers' use only, so whatever. Use '+' instead of '=' on the command line.
run-%:$(EXE_MAIN);$(CMD_MAIN) $(subst +,=,$*) $(POST_CMD_MAIN)

test:$(EXE_TEST);$(CMD_TEST) $(POST_CMD_TEST)
test-%:$(EXE_TEST);$(CMD_TEST) $* $(POST_CMD_TEST)
edit:$(EXE_EDIT);$(CMD_EDIT) --resources=src/data $(POST_CMD_EDIT)
edit-%:$(EXE_EDIT);$(CMD_EDIT) --resources=src/data $* $(POST_CMD_EDIT)

clean:;rm -rf mid out

export INPUTCFG
export MAINCFG
release:validate-release $(EXE_MAIN) platform-release
validate-release:;etc/tool/validate-release.sh
