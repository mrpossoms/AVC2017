#!/bin/bash

i=0
mkdir -p $1
read img_path
while [ $? -eq 0 ]
do
wget -bO $1/$i -q $img_path
((i++))
read img_path
done
