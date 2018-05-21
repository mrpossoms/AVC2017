#!/bin/sh

cat /dev/null > findings.mk

MAKE_LIB=
MAKE_INC=

echo "Finding libraries..."

for DEPENDENCY in $(cat depends); do
	echo $DEPENDENCY;
	EXACT_PATH=$(find /opt /usr/local -name "$DEPENDENCY" -print -quit)
	LIB_DIR=$(dirname $EXACT_PATH)
	echo $LIB_DIR	
	INC_DIR=$(dirname $LIB_DIR)/include
	

	if [ -z $LIB_DIR ]
	then
		printf "Couldn't find: %s\n" $DEPENDENCY;
		exit -1;
	fi

	MAKE_LIB+=$(printf " -L%s" $LIB_DIR);
	MAKE_INC+=$(printf " -I%s" $INC_DIR)
done

printf "LIB_PATHS+=%s\n" "$MAKE_LIB" >> findings.mk
printf "INC_PATHS+=%s" "$MAKE_INC" >> findings.mk

echo "Done!"
