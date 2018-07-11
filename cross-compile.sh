#!/bin/sh

export PREFIX=$TOOLCHAIN_PREFIX
export PATH=$TOOLCHAIN/bin:$PATH

echo $PREFIX
sleep 2

cd externals
./build.sh
if [ $? -eq 0 ]; then
	cd ..
	make bot-utils
fi

