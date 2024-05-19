#!/bin/bash
DL="https://github.com/openipc/firmware/releases/download/latest"

toolchain() {
	if [ ! -e toolchain/$1 ]; then
		wget -c -q --show-progress $DL/$1.tgz -P $PWD
		mkdir -p toolchain/$1
		tar -xf $1.tgz -C toolchain/$1 --strip-components=1 || exit 1
		rm -f $1.tgz
	fi
	GCC=$PWD/toolchain/$1/bin/arm-linux-gcc
}

if [ "$2" = "debug" ]; then
	OPT="-gdwarf-3"
else
	OPT="-Os -s"
fi

if [ "$1" = "divinus-musl" ]; then
	toolchain cortex_a7_thumb2-gcc13-musl-4_9
	make -B CC=$GCC OPT="$OPT"
elif [ "$1" = "divinus-muslhf" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-musl-4_9
	make -B CC=$GCC OPT="$OPT"
elif [ "$1" = "divinus-glibc" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-glibc-4_9
	make -B CC=$GCC OPT="$OPT -lm"
else
	echo "Usage: $0 [divinus-musl|divinus-muslhf|divinus-glibc]"
	exit 1
fi
