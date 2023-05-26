#!/bin/bash
# call this with the epos number as argument:
# ./debug.sh epos
# exp: ./debug 100
search=': '$1','
#echo $search
gline="$(grep "$search" out.l1dbg)"
#echo $gline
alinestr=${gline#*,}
read -r aline rest_of_string <<<"$alinestr"
echo $aline
aline=$((aline + 19))
head -n $aline out.l1asm | tail -20
