#!/bin/sh
set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap -DRTSP_TX_BUDGET='(1024 * 1024)' \
    "$TEST_DIR/rtp_nal_test.c" \
    "$PROJECT_ROOT/src/rtsp/mime.c" \
    "$PROJECT_ROOT/src/hal/tools.c" \
    "$PROJECT_ROOT/src/rtsp/rtsp.c" -lpthread -o rtp_nal_test
./rtp_nal_test
