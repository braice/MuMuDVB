#!/bin/sh
# Build script to android. Change the variables on "Path block" to the correct path.

API=8

# Path block
ANDROID_NDK=/home/ounao/android-ndk-r9c		# Android NDK path
SRC_DIR=/home/ounao/source/MuMuDVB		# Path to MuMuDVB source
INSTALL_DIR=/home/ounao/source/out		# Path to android MuMuDVB install
INCLUDE_DIR=/home/ounao/source/out/include	# Path to linux/dvb headers (obrigatory) and iconv.h (optional)
LIB_DIR=/home/ounao/source/out/lib		# Path to libiconv.a (optional)
# Path block

cd $SRC_DIR

export PATH="$ANDROID_NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86_64/bin/:$PATH"
export SYS_ROOT="$ANDROID_NDK/platforms/android-$API/arch-arm/"
export CC="arm-linux-androideabi-gcc --sysroot=$SYS_ROOT"
export CXX="arm-linux-androideabi-g++ --sysroot=$SYS_ROOT"
export CPP="arm-linux-androideabi-cpp --sysroot=$SYS_ROOT"
export LD="arm-linux-androideabi-ld"
export AR="arm-linux-androideabi-ar"
export RANLIB="arm-linux-androideabi-ranlib"
export STRIP="arm-linux-androideabi-strip"
export LDFLAGS="-L$LIB_DIR"
export CFLAGS="-I$INCLUDE_DIR"
export LIBS="-lc -lgcc -liconv"

./configure --host=arm-eabi --enable-android --prefix=$INSTALL_DIR

make
make install
