SRC := $(shell find src -name *.c)

divinus:
	$(CC) $(SRC) $(OPT) -I src -o $@
