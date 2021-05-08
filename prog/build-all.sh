#!/bin/bash
for j in *.l1asm
do
	l1asm $j -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $j && exit 1
done
for i in *.l1com
do
	l1pre $i out.l1com ~/l1vm/include/
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "preprocessor build failed: " && echo $i && exit 1
	l1com out -sizes 1000000 1000000000
	RETVAL=$?
	[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1

	# remove .l1com from file name and replace it by .l1obj
	outname=$(basename "$i" .l1com)
	outname="${outname}.l1obj"
	cp out.l1obj $outname

	# get .l1asm file
	outname=$(basename "$i" .l1asm)
	outname="${outname}.l1asm"
	cp out.l1asm $outname

	# get .l1dbg file
	outname=$(basename "$i" .l1dbg)
	outname="${outname}.l1dbg"
	cp out.l1dbg $outname
done
# cleanup .l1asm.l1dbg files
rm *.l1asm.l1dbg
exit 0
