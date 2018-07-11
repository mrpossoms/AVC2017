#!/bin/sh

export PREFIX=$(pwd)/emulation/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu
export PATH=$PREFIX/bin:$PATH

make bot-utils
