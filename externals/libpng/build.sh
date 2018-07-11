cd libpng*

if [ -z $PREFIX ]; then
	./configure
else
	./configure --prefix=$PREFIX --enable-arm-neon=no 
fi

make install -j4
