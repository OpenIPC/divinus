CFLAGS = -std=gnu99 -Wall -Wno-parentheses -I../src \
	-DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap
TESTS = network_test

all: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

network_test: network_test.c ../src/network.c ../src/hal/tools.c
	$(CC) $(CFLAGS) $^ -lpthread -o $@

.PHONY: all clean
