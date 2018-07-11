#!/bin/sh

for DEPEND in $(ls ./); do
	if [ -d $DEPEND ]; then
		for UNZIPPED in $(ls $DEPEND); do
			if [ -d $DEPEND/$UNZIPPED ]; then
				rm -rf $DEPEND/$UNZIPPED
			fi
		done
	fi
done	

rm .built
