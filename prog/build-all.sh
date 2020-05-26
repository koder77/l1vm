#!/bin/bash
for i in *.l1com
do
	l1com $i -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
done
for j in *.l1asm
do
	l1asm $j -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
exit 0
