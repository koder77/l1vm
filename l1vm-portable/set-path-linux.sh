#!/bin/bash
#L1VM
export L1VM_ROOT="$(pwd)"
export LD_LIBRARY_PATH="$L1VM_ROOT/bin-linux:$LD_LIBRARY_PATH"
export PATH="$L1VM_ROOT/bin-linux:$PATH"

echo "L1VM PATHs set to $L1VM_ROOT"
