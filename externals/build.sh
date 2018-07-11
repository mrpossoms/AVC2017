#!/bin/sh

for DEP in $(ls -d ./*); do
	cd $DEP
	./get.sh

	if [ $? -eq 0 ]; then
		./build.sh
	else
		echo "Failed to retrieve " $DEP
	fi

	cd -
done
