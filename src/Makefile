SRC = $(shell find ./ -not \( -path ./3rd -prune \) -name '*.c')
OBJ = ../divinus
INC = -I3rd/metalang99/include -I3rd/datatype99 -I3rd/interface99 -I3rd/slice99 -I3rd/smolrtsp/include -I3rd/smolrtsp/src -I3rd

.PHONY: clean divinus
divinus: $(OBJ)

$(OBJ): $(SRC:%.c=%.o)
	$(CC) $^ $(INC) -rdynamic $(OPT) -o $@

%.o: %.c Makefile
	$(CC) -c $< $(INC) $(OPT) -o $@

clean:
	rm -rf $(SRC:%.c=%.o) $(OBJ)
