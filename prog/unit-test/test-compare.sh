#!/bin/bash
if cmp --silent -- $1 $2; then
  echo "test passed, OK!"
  exit 0
else
  echo "ERROR: test failed!"
  exit 1
fi
