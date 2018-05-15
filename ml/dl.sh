#!/bin/bash

read img_path
while [ $? -eq 0 ]
do
	if [ -z $1 ]
	then
		curl -G $img_path
	else
		name=$(hexdump -n 16 -e '4/4 "%08X" 1 "\n"' /dev/random)
		mkdir -p $1
		wget -O $1/$name -q $img_path
		echo $1/$name
	fi
read img_path
done

exit 0
