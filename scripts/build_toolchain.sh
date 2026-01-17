#!/bin/bash
set -e

export PREFIX="/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

mkdir -p /root/src
cd /root/src

# Versions
BINUTILS_VER="2.39"
GCC_VER="12.2.0"

# Download
if [ ! -f "binutils-$BINUTILS_VER.tar.gz" ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.gz
fi
if [ ! -f "gcc-$GCC_VER.tar.gz" ]; then
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.gz
fi

# Extract
tar -xzf binutils-$BINUTILS_VER.tar.gz
tar -xzf gcc-$GCC_VER.tar.gz

# Build Binutils
mkdir -p build-binutils
cd build-binutils
../binutils-$BINUTILS_VER/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

# Build GCC
cd gcc-$GCC_VER
./contrib/download_prerequisites
cd ..
mkdir -p build-gcc
cd build-gcc
../gcc-$GCC_VER/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..
