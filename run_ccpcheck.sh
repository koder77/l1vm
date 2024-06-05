#!/bin/bash -e

# can be installed on Debian GNU/Linux with "sudo apt install cppcheck"

cppcheck -q --enable=all --force --language=c --check-level=exhaustive $(find . \( -name "*.c*" -or -name  "*.h" \))
