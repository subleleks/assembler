#!/bin/bash
#
# Tests for our awesome SUBLEQ assembler.

TEMP_FILE="/tmp/thing.obj"

function test() {
	./subleq-asm "$1.asm" "$TEMP_FILE"

	diff "$1.obj" "$TEMP_FILE" >/dev/null 2>&1

	if [ "$?" -eq 0 ]
	then
		echo "[ OK ]: $1"
	else
		echo "[FAIL]: $1"
	fi
}

for file in tests/*.asm
do
	FILENAME=`echo $file | sed 's|\(.*\)\.asm|\1|'`
	test "$FILENAME"
done

