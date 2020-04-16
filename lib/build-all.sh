#!/bin/bash
for i in *.l1com
do
	l1com $i
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
done
for j in *.l1asm
do
	l1asm $j
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
exit 0
