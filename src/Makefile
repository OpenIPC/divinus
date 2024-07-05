OBJ := $(shell find ./ -name '*.c' | sed -e "s/\.c/\.o/")

%.o: %.c
	@mkdir -p ../obj/$(shell dirname $<)
	$(CC) $< -I src $(OPT) -c -o ../obj/$@

divinus: $(OBJ)
	$(CC) $(addprefix ../obj/,$^) -I src -rdynamic $(OPT) -o ../$@
