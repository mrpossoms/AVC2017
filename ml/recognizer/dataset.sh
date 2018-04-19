#!/bin/bash

class=$1
read search_term
while [ $? -eq 0 ]
do
	python3 ./img.py $search_term $2 | ./dl.sh imgs/$1 | python3 crop.py class=$1 $2
read search_term
done
