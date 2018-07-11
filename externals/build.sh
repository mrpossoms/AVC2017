#!/bin/sh

if [ -f .built ]; then
	exit 0
fi

for DEP in $(ls -d ./*); do
	if [ -f $DEP ]; then
		continue
	fi

	cd $DEP
	./get.sh

	if [ $? -eq 0 ]; then
		./build.sh
	else
		echo "Failed to retrieve " $DEP
	fi

	cd ..
done

touch .built
