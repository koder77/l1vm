#!/bin/bash
#L1VM
export LD_LIBRARY_PATH="./bin:$LD_LIBRARY_PATH"
export PATH="./bin:$PATH"
export L1VM_ROOT="."

if [ "$#" -lt 1 ]
then
  echo "build.sh <program-name>"
  exit 1
fi

l1vm-build.sh $1 $2 $3 $4 $5 $6
