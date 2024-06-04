#!/bin/sh
DL="https://github.com/openipc/firmware/releases/download/latest"

toolchain() {
	if [ ! -e toolchain/$1 ]; then
		wget -c -q $DL/$1.tgz -P $PWD
		mkdir -p toolchain/$1
		tar -xf $1.tgz -C toolchain/$1 --strip-components=1 || exit 1
		rm -f $1.tgz
	fi
	make -C src -B CC=$PWD/toolchain/$1/bin/$2-linux-gcc OPT="$OPT $3"
}

if [ "$2" = "debug" ]; then
	OPT="-DDEBUG -gdwarf-3"
else
	OPT="-Os -s"
fi

if [ "$1" = "arm-musl" ]; then
	toolchain cortex_a7_thumb2-gcc13-musl-4_9 arm
elif [ "$1" = "arm-muslhf" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-musl-4_9 arm
elif [ "$1" = "arm-glibc" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-glibc-4_9 arm -lm
elif [ "$1" = "mips-musl" ]; then
	toolchain mips_xburst-gcc13-musl-3_10 mipsel
else
	echo "Usage: $0 [arm-musl|arm-muslhf|arm-glibc|mips-musl]"
fi
