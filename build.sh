#!/bin/sh
DL="https://github.com/OpenIPC/firmware/releases/download/toolchain"
EXT="tgz"
PRE="linux"

toolchain() {
	if [ ! -e toolchain/$1 ]; then
		wget -c -q $DL/$1.$EXT -P $PWD
		mkdir -p toolchain/$1
		if [ "$EXT" = "zip" ]; then
			unzip $1.$EXT || exit 1
			chmod -R +x toolchain/$1/bin
		else
			tar -xf $1.$EXT -C toolchain/$1 --strip-components=1 || exit 1
		fi
		rm -f $1.$EXT
	fi
	make -j $(nproc) -C src CC=$PWD/toolchain/$1/bin/$2-$PRE-gcc OPT="$OPT $3"
}

if [ "$2" = "debug" ]; then
	OPT="-DDEBUG -gdwarf-3"
else
	OPT="-Os -s"
fi

if [ "$1" = "arm-glibc" ] || [ "$1" = "hisi-v4a" ]; then
	toolchain toolchain.hisilicon-hi3516cv500 -lm
elif [ "$1" = "arm-musl3" ] || [ "$1" = "hisi-v2a" ] || [ "$1" = "hisi-v3a" ]; then
    toolchain toolchain.hisilicon-hi3516av100 arm
elif [ "$1" = "arm-musl4" ] || [ "$1" = "hisi-v4" ]; then
    toolchain toolchain.hisilicon-hi3516ev200 arm
elif [ "$1" = "arm9-glibc" ] || [ "$1" = "ak39xx" ]; then
	DL="https://github.com/Lamobo/Lamobo-D1/raw/master/compiler"
	EXT="tar.bz2"
	PRE="none-linux-gnueabi"
	toolchain arm-2009q3-67-arm-none-linux-gnueabi-i686-pc-linux-gnu arm "-ldl -lm -lpthread -lrt -std=gnu99"
elif [ "$1" = "arm9-musl3" ] || [ "$1" = "hisi-v1" ]; then
	toolchain toolchain.hisilicon-hi3516cv100 arm
elif [ "$1" = "arm9-musl4" ] || [ "$1" = "hisi-v2" ] || [ "$1" = "hisi-v3" ]; then
	toolchain toolchain.hisilicon-hi3516cv200 arm
elif [ "$1" = "arm9-uclibc" ] || [ "$1" = "gm813x" ]; then
	toolchain toolchain.grainmedia-gm8136 arm
elif [ "$1" = "armhf-glibc" ] || [ "$1" = "star6e" ]; then
	toolchain toolchain.sigmastar-infinity6e arm -lm
elif [ "$1" = "armhf-musl" ] || [ "$1" = "star6" ]; then
	toolchain toolchain.sigmastar-infinity6 arm
elif [ "$1" = "mips-musl" ] || [ "$1" = "inge-t31" ]; then
	toolchain toolchain.ingenic-t31 mipsel
elif [ "$1" = "riscv64-musl" ]; then
	DL="https://toolchains.bootlin.com/downloads/releases/toolchains/riscv64-lp64d/tarballs"
	EXT="tar.xz"
	toolchain riscv64-lp64d--musl--stable-2024.05-1 riscv64
else
	echo "Usage: $0 [arm-glibc|arm-musl3|arm-musl4|arm9-musl3|arm9-musl4|arm9-uclibc|armhf-glibc|armhf-musl|mips-musl|riscv64-musl|"
	echo "           ak39xx|gm813x|hisi-v1|hisi-v2|hisi-v2a|hisi-v3|hisi-v3a|hisi-v4|hisi-v4a|inge-t31|star6|star6e]"
	echo "          (debug)"
fi
