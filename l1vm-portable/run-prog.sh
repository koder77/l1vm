#!/bin/bash
#L1VM
export LD_LIBRARY_PATH="./bin:$LD_LIBRARY_PATH"
export PATH="$PATH:./bin"
export L1VM_ROOT="."

if [ "$#" -lt 1 ]
then
  echo "run-prog.sh <program-name>"
  exit 1
fi

l1vm $1
