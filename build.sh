#!/bin/sh
DL="https://github.com/openipc/firmware/releases/download/latest"
EXT="tgz"
PRE="linux"

toolchain() {
	if [ ! -e toolchain/$1 ]; then
		wget -c -q $DL/$1.$EXT -P $PWD
		mkdir -p toolchain/$1
		if [ "$EXT" = "zip" ]; then
			unzip $1.$EXT || exit 1
		else
			tar -xf $1.$EXT -C toolchain/$1 --strip-components=1 || exit 1
		fi
		rm -f $1.$EXT
	fi
	make -j $(nproc) -C src -B CC=$PWD/toolchain/$1/bin/$2-$PRE-gcc OPT="$OPT $3"
}

if [ "$2" = "debug" ]; then
	OPT="-DDEBUG -gdwarf-3"
else
	OPT="-Os -s"
fi

if [ "$1" = "arm-musl" ]; then
        toolchain cortex_a7_thumb2-gcc13-musl-4_9 arm
elif [ "$1" = "arm9-glibc" ]; then
	DL="https://github.com/Lamobo/Lamobo-D1/raw/master/compiler"
	EXT="tar.bz2"
	PRE="none-linux-gnueabi"
	toolchain arm-2009q3-67-arm-none-linux-gnueabi-i686-pc-linux-gnu arm "-ldl -lm -lpthread -lrt -std=gnu99"
elif [ "$1" = "arm9-musl3" ]; then
	toolchain arm926t-gcc13-musl-3_0 arm
elif [ "$1" = "arm9-musl4" ]; then
	toolchain arm926t-gcc13-musl-4_9 arm
elif [ "$1" = "arm9-uclibc" ]; then
	toolchain arm926t-gcc13-uclibc-3_3 arm
elif [ "$1" = "armhf-glibc" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-glibc-4_9 arm -lm
elif [ "$1" = "armhf-musl" ]; then
	toolchain cortex_a7_thumb2_hf-gcc13-musl-4_9 arm
elif [ "$1" = "mips-musl" ]; then
	toolchain mips_xburst-gcc13-musl-3_10 mipsel
elif [ "$1" = "riscv64-musl" ]; then
	DL="https://toolchains.bootlin.com/downloads/releases/toolchains/riscv64-lp64d/tarballs"
	EXT="tar.xz"
	toolchain riscv64-lp64d--musl--stable-2024.05-1 riscv64
else
	echo "Usage: $0 [arm-musl|arm9-musl3|arm9-musl4|arm9-uclibc|armhf-glibc|armhf-musl|mips-musl|riscv64-musl]"
fi
