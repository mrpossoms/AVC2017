cd libpng*

if [ -z $TOOLCHAIN_PREFIX ]; then
	./configure
else
	./configure --prefix=$TOOLCHAIN_PREFIX --host=$(gcc -dumpmachine) --enable-arm-neon=no 
fi

make install -j4
