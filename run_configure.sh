#!/usr/bin/env bash

	# --as=clang --cc=clang --cxx=clang++ --objcc=clang --dep-cc=clang \
	#--extra-cflags="-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard" \

set -x
./configure \
	--ld="gcc -fuse-ld=lld -Wl,-z,notext" \
	--extra-cflags="-march=native -mfpu=neon-vfpv4 -mfloat-abi=hard" \
	--enable-thumb --enable-hardcoded-tables --disable-runtime-cpudetect \
	--enable-gpl --enable-libx264 --enable-libvpx \
	--enable-libfontconfig --enable-libfreetype \
	--enable-libsrt --enable-libgpiod
