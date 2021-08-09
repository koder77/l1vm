#!/bin/sh
if [ $# -eq 0 ]
  then
    echo "usage: run.sh filename"
	exit 1
fi

curl -O http://localhost:2000/web/$1.l1obj.bz2
curl -O http://localhost:2000/web/$1.gpg

if ! gpg --verify $1.gpg $1.l1obj.bz2; then
	echo "ERROR: file signature not valid!"
	exit 1
fi

echo "file signature ok, running program..."
l1vm $1
