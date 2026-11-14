#!/bin/sh
set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap \
    "$TEST_DIR/network_test.c" "$PROJECT_ROOT/src/network.c" \
    "$PROJECT_ROOT/src/hal/tools.c" -lpthread -o network_test
./network_test
