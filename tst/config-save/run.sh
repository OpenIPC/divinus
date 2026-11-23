#!/bin/sh
set -eu

CC=${CC:-cc}
# app_config.c has warnings of its own that are not the subject of this test
CFLAGS=${CFLAGS:--std=gnu99 -w}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap \
    "$TEST_DIR/config_test.c" "$PROJECT_ROOT/src/app_config.c" \
    "$PROJECT_ROOT/src/hal/config.c" "$PROJECT_ROOT/src/hal/tools.c" \
    -o config_test
./config_test
