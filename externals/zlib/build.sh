cd zlib*

if [ -z $PREFIX ]; then
	./configure
else
	./configure --prefix=$PREFIX/aarch64-linux-gnu
fi
make install
