# Tests of the actual private RTSP parameter cache and public RTP packetizer.
# No RTSP server, sockets, camera, hardware or vendor binary is run.
CC ?= cc
BUILD_DIR ?= build
# Optional existing output directory for serialized framing-test packets.
FRAMING_CAPTURE_DIR ?=
COMMON_FLAGS = -std=gnu11 -ffunction-sections -fdata-sections
SPROP_SOURCES = test_rtsp_sprop.c ../src/rtsp/mime.c
FRAMING_SOURCES = test_rtp_framing.c ../src/rtsp/mime.c

ifeq ($(shell uname -s),Darwin)
COMMON_FLAGS += -Wno-gnu-designator
GC_SECTIONS = -Wl,-dead_strip
else
GC_SECTIONS = -Wl,--gc-sections
endif

.PHONY: test test-native test-sanitize test-sprop-native test-sprop-sanitize test-framing-native test-framing-sanitize
test: test-native test-sanitize
test-native: test-sprop-native test-framing-native
test-sanitize: test-sprop-sanitize test-framing-sanitize

test-sprop-native:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O2 $(SPROP_SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/sprop-native
	$(BUILD_DIR)/sprop-native

test-sprop-sanitize:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(SPROP_SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/sprop-sanitized
	$(BUILD_DIR)/sprop-sanitized
	$(BUILD_DIR)/sprop-sanitized --short-vps-two

test-framing-native:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O2 $(FRAMING_SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/framing-native
	$(BUILD_DIR)/framing-native $(if $(FRAMING_CAPTURE_DIR),--capture $(FRAMING_CAPTURE_DIR)/framing-native.bin)

test-framing-sanitize:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_FLAGS) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(FRAMING_SOURCES) $(LDFLAGS) $(GC_SECTIONS) -pthread -o $(BUILD_DIR)/framing-sanitized
	$(BUILD_DIR)/framing-sanitized $(if $(FRAMING_CAPTURE_DIR),--capture $(FRAMING_CAPTURE_DIR)/framing-sanitized.bin)
