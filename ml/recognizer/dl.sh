#!/bin/bash

i=0
mkdir -p $1
read img_path
while [ $? -eq 0 ]
do
	if [ -z $1 ]
	then
		curl -G $img_path
	else
		wget -bO $1/$i -q $img_path
	fi
((i++))
read img_path
done
