#!/bin/sh
set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=gnu99 -Wall -Wno-parentheses}

# gpio.c is included by the test itself, so it can see the fake sysfs
$CC $CFLAGS \
    -I"$PROJECT_ROOT/src" -I"$PROJECT_ROOT/tst" \
    "$TEST_DIR/gpio_test.c" -o gpio_test
./gpio_test
GPIO_TEST_UPPER=1 ./gpio_test
