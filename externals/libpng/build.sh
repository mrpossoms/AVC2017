cd libpng*

if [ -z $PREFIX ]; then
	./configure
else
	./configure --prefix=$PREFIX/aarch64-linux-gnu --enable-arm-neon=no --host=aarch64
fi

make install -j4
