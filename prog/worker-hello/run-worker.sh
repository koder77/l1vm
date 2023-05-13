#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "usage: run-worker.sh filename"
	exit 1
fi

./run.sh $1
exit 0
