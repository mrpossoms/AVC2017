#!/bin/sh
BOT_ADDR=172.20.10.12
BIN=bin/armv8l-linux-gnueabihf

if [ $1 = "all" ]; then
	PROGS=$(ls -p $BIN | grep -v /)
elif [ $1 = "nn" ]; then
	scp /etc/bot/predictor/model/* root@172.20.10.12:/etc/bot/predictor/model
	exit 0
else
	PROGS=$1
fi

for PROG in $PROGS; do
	scp $BIN/$PROG root@$BOT_ADDR:~/AVC2017/bin/arm-linux-gnueabihf/
done

