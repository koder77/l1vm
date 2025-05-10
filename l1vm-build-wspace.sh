#!/bin/bash
echo -n "building: " && echo $1

secondinclude=${PWD}
secondinclude+='/'
l1pre $1.l1com out.l1com ~/l1vm/include/ $secondinclude -wdeprecated -wspaces
RETVAL=$?
[ $RETVAL -ne 0 ] && echo "preprocessor build failed: " && echo $i && exit 1
l1com out $2 $3 $4 $5 $6 $7
RETVAL=$?
[ $RETVAL -ne 0 ] && echo "build failed!" && exit 1

if [ "$2" = "-pack" ]; then
	# remove .l1com from file name and replace it by .l1obj.bz2
	outname=${1%.l1com}
	outname="${outname}.l1obj.bz3"
	cp out.l1obj.bz3 $outname
else
	# remove .l1com from file name and replace it by .l1obj
	outname=${1%.l1com}
	outname="${outname}.l1obj"
	cp out.l1obj $outname
fi

echo "copying files..."

# get .l1asm file
outname=""
outname=${1%.l1asm}
outname="${outname}.l1asm"
cp out.l1asm $outname

# get .l1dbg file
outname=""
outname=${1%.l1com}
outname="${outname}.l1dbg"
cp out.l1dbg $outname

# get .md markdown docu file
if test -e "out.md"; then
outname=""
outname=${1%.md}
outname="${outname}.md"
cp out.md $outname
p $outname "$L1VM_ROOT/man/"
rm $outname
fi

#cleanup
rm out.l1com
rm out.l1obj

if test -e "out.md"; then
rm out.md
fi
if test -e "tmp.md"; then
rm tmp.md
fi
