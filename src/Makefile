SRC := $(shell find ./ -name '*.c')

divinus:
	$(CC) $(SRC) -I src -rdynamic -Wl,--wrap=mmap $(OPT) -o ../$@