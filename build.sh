#!/bin/bash

l1pre $1 out.l1com ~/l1vm/include/
RETVAL=$?
[ $RETVAL -ne 0 ] && echo "preprocessor build failed: " && echo $i && exit 1
l1com out
RETVAL=$?
[ $RETVAL -ne 0 ] && echo "build failed: " && echo $i && exit 1
# remove .l1com from file name and replace it by .l1obj
outname=${1%.l1com}
outname="${outname}.l1obj"
cp out.l1obj $outname

# get .l1asm file
outname=$(basename "$i" .l1asm)
outname="${outname}.l1asm"
cp out.l1asm $outname

# get .l1dbg file
outname=${1%.l1com}
outname="${outname}.l1dbg"
cp out.l1dbg $outname
