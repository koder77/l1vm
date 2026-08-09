#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "usage: run.sh filename"
	exit 1
fi

sh download.sh $1

l1vm $1
