#!/bin/bash
for i in *.l1com
do
	l1pre $i out.l1com ~/l1vm/include/
	l1com out
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
	# remove .l1com from file name and replace it by .l1obj
	outname=${i%.l1com}
	outname="${outname}.l1obj"
	cp out.l1obj $outname
done
for j in *.l1asm
do
	l1asm $j
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
exit 0
