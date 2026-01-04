#!/bin/bash
#L1VM
export L1VM_ROOT="$(pwd)"
export LD_LIBRARY_PATH="$L1VM_ROOT/bin-windows:$LD_LIBRARY_PATH"
export PATH="$L1VM_ROOT/bin-windows:$PATH"

echo "L1VM PATHs set to $L1VM_ROOT"
