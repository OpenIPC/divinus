# Synthetic tests of the actual private RTSP parameter-cache implementation.
# No RTSP server, sockets, camera, hardware or vendor binary is run.
CC ?= cc
BUILD_DIR ?= build
COMMON_FLAGS = -std=gnu11 -ffunction-sections -fdata-sections
SOURCES = test_rtsp_sprop.c ../src/rtsp/mime.c

ifeq ($(shell uname -s),Darwin)
COMMON_FLAGS += -Wno-gnu-designator
GC_SECTIONS = -Wl,-dead_strip
else
GC_SECTIONS = -Wl,--gc-sections
endif

.PHONY: test test-native test-sanitize
test: test-native test-sanitize

test-native:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O2 $(SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/sprop-native
	$(BUILD_DIR)/sprop-native

test-sanitize:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/sprop-sanitized
	$(BUILD_DIR)/sprop-sanitized
	$(BUILD_DIR)/sprop-sanitized --short-vps-two
