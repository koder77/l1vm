#!/bin/sh
# Build and run the Java bindings test suite.
#
# Requires a JDK 21+ with the Foreign Function & Memory API. On JDK 21 that
# API is still a preview feature, hence --enable-preview.
#
# Usage:  ./run-tests.sh
#         VECDB_LIBRARY=/path/to/libvecdb.so ./run-tests.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
cd "$here"

# Locate libvecdb for the tests if the caller did not point us at it.
if [ -z "${VECDB_LIBRARY:-}" ]; then
    for cand in \
        "$here/libvecdb.so" \
        "$here/../../libvecdb.so" \
        "$here/../../../libvecdb.so"
    do
        if [ -f "$cand" ]; then
            export VECDB_LIBRARY=$(cd "$(dirname "$cand")" && pwd)/$(basename "$cand")
            break
        fi
    done
fi

JAVAC=${JAVAC:-javac}
JAVA=${JAVA:-java}

# JDK 21 needs --enable-preview; later releases may not, but passing it is
# harmless there only if the class files were built the same way, so keep it.
PREVIEW="--enable-preview"
NATIVE="--enable-native-access=ALL-UNNAMED"

rm -rf build
mkdir -p build/classes

echo "compiling..."
$JAVAC --release 21 $PREVIEW -d build/classes $(find src -name '*.java')

echo "running..."
$JAVA $PREVIEW $NATIVE -cp build/classes vecdb.VecDbTest
