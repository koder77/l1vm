#!/bin/sh
if [ $# -eq 0 ]
  then
    echo "usage: get-obj.sh filename"
	exit 1
fi

curl -O http://localhost:2000/web/$1.l1obj
curl -O http://localhost:2000/web/$1.gpg

if ! gpg --verify $1.gpg $1.l1obj; then
	echo "ERROR: file signature not valid!"
	exit 1
fi

echo "file signature ok, running program..."
l1vm $1
