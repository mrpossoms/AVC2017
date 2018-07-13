cd zlib*

if [ -z $TOOLCHAIN_PREFIX ]; then
	./configure
else
	./configure --prefix=$TOOLCHAIN_PREFIX
fi
make install
