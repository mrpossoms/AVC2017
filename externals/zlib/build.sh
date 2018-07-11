cd zlib*

if [ -z $PREFIX ]; then
	./configure
else
	./configure --prefix=$PREFIX
fi
make install
