#!/bin/bash
for i in *.l1com
do
	l1pre $i out.l1com ~/l1vm/include/
	l1com out
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
	outname="${i}.l1obj"
	cp out.l1obj $outname
done
for j in *.l1asm
do
	l1asm $j
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
exit 0
