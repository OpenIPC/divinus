SRC = $(shell find ./ -name '*.c')
OBJ = ../divinus
INC = -I.. -I../3rd/metalang99/include -I../3rd/datatype99 -I../3rd/interface99 -I../3rd/slice99 -I../3rd/smolrtsp/include -I../3rd/smolrtsp/src
GIT_REV := $(shell git rev-parse --short HEAD)$(shell git diff --quiet HEAD || echo "-mod")

.PHONY: clean divinus
divinus: $(OBJ)

$(OBJ): $(SRC:%.c=%.o)
	$(CC) $^ $(INC) -rdynamic $(OPT) -DGIT_REV=\"$(GIT_REV)\" -o $@

%.o: %.c Makefile
	$(CC) -c $< $(INC) $(OPT) -DGIT_REV=\"$(GIT_REV)\" -o $@

clean:
	rm -rf $(SRC:%.c=%.o) $(OBJ)
