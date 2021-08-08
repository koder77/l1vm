#!/bin/sh
if [ $# -eq 0 ]
  then
    echo "usage: sign-obj.sh filename"
	exit 1
fi

gpg --detach-sign --default-key YOURKEYID -o $1.gpg $1.l1obj.bz2
