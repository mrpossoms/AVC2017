#!/bin/sh

export PATH=$TOOLCHAIN/bin:$PATH

cd externals
./build.sh
if [ $? -eq 0 ]; then
	cd ..
	make bot-utils
fi

