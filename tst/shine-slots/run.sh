#!/bin/sh
set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    "$TEST_DIR/shine_test.c" "$PROJECT_ROOT"/src/lib/shine/*.c -lm -o shine_test
./shine_test
