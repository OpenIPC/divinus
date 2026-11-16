#!/bin/sh
set -eu

CC=${CC:-cc}
# onvif.c and app_config.h have warnings of their own that are not the subject of this test
CFLAGS=${CFLAGS:--std=gnu99 -w}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap \
    "$TEST_DIR/onvif_test.c" "$PROJECT_ROOT/src/onvif.c" \
    "$PROJECT_ROOT/src/hal/tools.c" -lpthread -o onvif_test
./onvif_test
