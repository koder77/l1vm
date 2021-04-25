#!/bin/bash -e

# can be installed on Debian GNU/Linux with "sudo apt install cppcheck"

cppcheck -q --enable=all --force --language=c $(find . \( -name "*.c" -or -name  "*.h" \))
