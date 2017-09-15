run:$(EXE_MAIN);$(CMD_MAIN)
run-%:$(EXE_MAIN);$(CMD_MAIN) $*
test:$(EXE_TEST);$(CMD_TEST)
test-%:$(EXE_TEST);$(CMD_TEST) $*
edit:$(EXE_EDIT);$(CMD_EDIT)
edit-%:$(EXE_EDIT);$(CMD_EDIT) $*

clean:;rm -rf mid out
