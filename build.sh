#!/bin/bash
DL="https://github.com/openipc/firmware/releases/download/latest"

if [[ "$1" == *"-musl" ]]; then
    CC=cortex_a7_thumb2-gcc13-musl-4_9
    GCC=$PWD/toolchain/$CC/bin/arm-linux-gcc
elif [[ "$1" == *"-muslhf" ]]; then
	CC=cortex_a7_thumb2_hf-gcc13-musl-4_9
    GCC=$PWD/toolchain/$CC/bin/arm-linux-gcc
elif [[ "$1" == *"-glibc" ]]; then
	CC=cortex_a7_thumb2_hf-gcc13-glibc-4_9
    GCC=$PWD/toolchain/$CC/bin/arm-linux-gcc
fi

if [ -n $CC ] && [ ! -e toolchain/$CC ]; then
	wget -c -q --show-progress $DL/$CC.tgz -P $PWD
	mkdir -p toolchain/$CC
	tar -xf $CC.tgz -C toolchain/$CC --strip-components=1 || exit 1
	rm -f $CC.tgz
fi

if [ "$2" = "debug" ]; then
	OPT="-gdwarf-3"
else
	OPT="-Os -s"
fi

if [ "$1" = "divinus-musl" ]; then
	make -C src -B CC=$GCC OPT="$OPT" $1
elif [ "$1" = "divinus-muslhf" ]; then
	make -C src -B CC=$GCC OPT="$OPT" $1
elif [ "$1" = "divinus-glibc" ]; then
	make -C src -B CC=$GCC OPT="$OPT" $1
else
	echo "Usage: $0 [divinus-musl|divinus-muslhf|divinus-glibc]"
	exit 1
fi