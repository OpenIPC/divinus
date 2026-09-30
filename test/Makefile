CFLAGS = -std=gnu99 -Wall -Wno-parentheses -I../src \
	-DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap
TESTS = rtsp_con_test

all: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

# rtsp.c is included by the test to reach its static functions
rtsp_con_test: rtsp_con_test.c ../src/rtsp/mime.c ../src/hal/tools.c ../src/rtsp/rtsp.c $(wildcard ../src/rtsp/*.h)
	$(CC) $(CFLAGS) $(filter-out ../src/rtsp/rtsp.c %.h,$^) -lpthread -o $@

.PHONY: all clean
