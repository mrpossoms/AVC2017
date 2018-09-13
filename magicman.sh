#!/bin/sh

touch_dir() {
	if [ ! -d $1 ]; then
		mkdir -p $1
	fi
}

# compute the checksum for the current
# state of the structures header
new_sum=$(cksum src/structs.h | awk '{split($0,a," "); print a[1]}')

mig_files="structs.h sys.h sys.c"

if [ -f magic ]; then
	old_sum=$(cat magic)
	
	# structs have been updated
	# save the old magic for migration
	if [ new_sum != old_sum ]; then
		touch_dir .migration
		for file in $mig_files; do
			git show HEAD^^^:src/$file > .migration/$file
		done
		mv magic .migration/magic
	fi
fi

echo $new_sum > magic
