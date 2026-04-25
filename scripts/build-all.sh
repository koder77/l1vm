#!/bin/bash
# build demo programs (recursive & path-safe)

# Immer vom Basisverzeichnis ausgehen
BASE_DIR="$HOME/l1vm"
cd "$BASE_DIR" || exit 1

echo "Starting build in $BASE_DIR..."

# 1. Alle .l1asm Dateien finden
# Wir suchen im Verzeichnis "prog" relativ zum BASE_DIR
find prog -type f -name "*.l1asm" | while read -r i; do
    # Pfad ohne Endung (z.B. prog/linter/test)
    filepath="${i%.l1asm}"
    echo "Assembling: $i"
    l1asm "$filepath"
done

# 2. Alle .l1com Dateien finden
find prog -type f -name "*.l1com" | while read -r i; do
    filepath="${i%.l1com}"
    echo "Building: $i"
    # Aufruf des Build-Scripts mit dem Pfad relativ zu ~/l1vm
    l1vm-build.sh "$filepath" -sizes 1000000 1000000000
done

# 3. Debug-Dateien aufräumen
find prog -type f -name "*.l1asm.l1dbg" -delete
find prog -type f -name "*.l1dbg" -delete

echo "Build finished."
