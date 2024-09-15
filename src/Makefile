SRC = $(shell find ./ -name '*.c')
OBJ = ../divinus

.PHONY: clean divinus
divinus: $(OBJ)

$(OBJ): $(SRC:%.c=%.o)
	$(CC) $^ -rdynamic $(OPT) -o $@

%.o: %.c Makefile
	$(CC) -c $< -o $@

clean:
	rm -rf $(SRC:%.c=%.o) $(OBJ)
