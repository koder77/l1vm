#!/bin/bash
# build demo programs (recursive & path-safe)

# set base dir
BASE_DIR="$HOME/l1vm"
cd "$BASE_DIR" || exit 1

echo "Starting build in $BASE_DIR..."

# 1. Search all .l1asm files
find prog -type f -name "*.l1asm" | while read -r i; do
    # Pfad ohne Endung (z.B. prog/linter/test)
    filepath="${i%.l1asm}"
    echo "Assembling: $i"
    l1asm "$filepath"
done

# 2. Search all .l1com files
find prog -type f -name "*.l1com" | while read -r i; do
    filepath="${i%.l1com}"
    echo "Building: $i"
    # Aufruf des Build-Scripts mit dem Pfad relativ zu ~/l1vm
    l1vm-build.sh "$filepath" -sizes 1000000 1000000000
done

# 3. Clean up the debug files
find prog -type f -name "*.l1asm.l1dbg" -delete
find prog -type f -name "*.l1dbg" -delete

echo "Build finished."
