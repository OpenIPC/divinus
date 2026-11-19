#!/bin/sh
set -eu

CC=${CC:-cc}
# AddressSanitizer is the point here: the bug this guards is a write past a
# stack buffer, which a plain build would let pass unnoticed.
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses -fsanitize=address -fno-omit-frame-pointer}

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap \
    "$TEST_DIR/osd_test.c" "$PROJECT_ROOT/src/region.c" "$PROJECT_ROOT/src/lib/spng.c" \
    "$PROJECT_ROOT/src/lib/miniz/miniz.c" "$PROJECT_ROOT/src/lib/miniz/miniz_tinfl.c" \
    "$PROJECT_ROOT/src/lib/miniz/miniz_tdef.c" \
    -o osd_test -lm -lpthread
./osd_test
