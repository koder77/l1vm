#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "statify.sh <program>"
    exit 1
fi

ldd $1 | grep "=> /" | awk '{print $3}' | xargs -I '{}' cp -v '{}' ./
exit 0
