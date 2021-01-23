#!/bin/bash
for j in *.l1asm
do
	l1asm $j -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
for i in *.l1com
do
	l1pre $1 out.l1com ~/l1vm/include/
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "preprocessor build failed: " && echo $i && exit 1
	l1com out -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
done
# cleanup .l1asm.l1dbg files
rm *.l1asm.l1dbg
exit 0
