CFLAGS = -std=gnu99 -Wall -Wno-parentheses -I../src \
	-DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap
TESTS = soap_test onvif_event_test motion_core_test

all: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

soap_test: soap_test.c ../src/soap.c ../src/hal/tools.c
	$(CC) $(CFLAGS) $^ -o $@

onvif_event_test: onvif_event_test.c ../src/onvif_event.c
	$(CC) $(CFLAGS) $^ -lpthread -o $@

motion_core_test: motion_core_test.c ../src/motion_core.c
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: all clean
