#!/bin/bash
# call this with the epos number as argument:
# ./l1vm-var.sh variable-address
# exp: ./debug.sh 100
if [[ $# -eq 0 ]] ; then
    echo 'l1vm-var.sh <variable address>'
    exit 1
fi

search=''$1'Q'
#echo $search
gline="$(grep "$search" out.l1asm)"
#echo $gline
line="$(grep -n "$search" out.l1asm | cut -d : -f 1)"
#echo $line
line=$((line))
head -n $line out.l1asm | tail -2
exit 0
