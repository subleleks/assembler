#!/bin/bash
#
# Tests for our awesome SUBLEQ assembler.

TEMP_FILE=/tmp/thing.obj

function test() {
	./subleq-asm "$1.asm" "$TEMP_FILE"

	diff "$1.obj" "$TEMP_FILE" 1>&2 2>/dev/null

	if [ "$?" -eq 0 ]
	then
		echo "[ OK ]: $1"
	else
		echo "[FAIL]: $1"
	fi
}

echo "[MAKE]"
make

echo "[TEST]"
for file in tests/*.asm
do
	FILENAME=`echo $file | sed 's|\(.*\)\.asm|\1|'`
	test "$FILENAME"
done

