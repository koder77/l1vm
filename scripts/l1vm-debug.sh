#!/bin/bash
# call this with the epos number as argument:
# ./debug.sh epos
# exp: ./l1vm-debug.sh 100

if [[ $# -eq 0 ]] ; then
    echo 'l1vm-debug.sh <epos>'
    exit 1
fi

search=': '$1','
#echo $search
gline="$(grep "$search" out.l1dbg)"
#echo $gline
alinestr=${gline#*,}
read -r aline rest_of_string <<<"$alinestr"
echo $aline
aline=$((aline + 19))
head -n $aline out.l1asm | tail -20
exit 0
