#!/bin/sh
if [ $# -eq 0 ]; then
    >&2 echo "No arguments provided! Need full l1vm program archive name!"
    exit 1
fi

tar xjf $1 -C ~/l1vm/
echo "Package installed!"
exit 0
