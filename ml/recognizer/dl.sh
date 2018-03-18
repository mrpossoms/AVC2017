#!/bin/bash

i=0
read img_path
while [ $? -eq 0 ]
do
	if [ -z $1 ]
	then
		curl -G $img_path
		#cat /tmp/img
	else
		mkdir -p $1
		wget -O $1/$i -q $img_path
		echo $1/$i
	fi
((i++))
read img_path
done
