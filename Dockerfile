FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    bison \
    flex \
    libgmp3-dev \
    libmpfr-dev \
    libmpc-dev \
    texinfo \
    wget \
    xorriso \
    grub-pc-bin \
    grub-common \
    mtools \
    qemu-system-x86 \
    git \
    python3 \
    cmake

# Build cross-compiler (binutils + gcc)
WORKDIR /root/toolchain
COPY scripts/build_toolchain.sh .
RUN chmod +x build_toolchain.sh && ./build_toolchain.sh

ENV PATH="/opt/cross/bin:${PATH}"

WORKDIR /os
CMD ["make", "iso"]
