#!/bin/sh
set -eu

CC=${CC:-cc}
# AddressSanitizer is the point here: the bug this guards is a write past a
# stack buffer, which a plain build would let pass unnoticed. It is linked
# non-PIE where the compiler allows: on a kernel with high ASLR entropy
# (vm.mmap_rnd_bits=32) a PIE binary lands where ASan wants its shadow memory,
# and ASan spins on DEADLYSIGNAL before main instead of running the test.
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses -fsanitize=address -fno-omit-frame-pointer}
case $CFLAGS in
    *-fsanitize=address*)
        if printf 'int main(void){return 0;}\n' |
            $CC -fsanitize=address -no-pie -x c - -o /dev/null 2>/dev/null; then
            CFLAGS="$CFLAGS -no-pie"
        fi
        ;;
esac

$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    -DCLOCK_MONOTONIC_COARSE=CLOCK_MONOTONIC -Dmmap64=mmap \
    "$TEST_DIR/osd_test.c" "$PROJECT_ROOT/src/region.c" "$PROJECT_ROOT/src/lib/spng.c" \
    "$PROJECT_ROOT/src/lib/miniz/miniz.c" "$PROJECT_ROOT/src/lib/miniz/miniz_tinfl.c" \
    "$PROJECT_ROOT/src/lib/miniz/miniz_tdef.c" \
    -o osd_test -lm -lpthread
./osd_test
