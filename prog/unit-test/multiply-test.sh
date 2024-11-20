#!/bin/bash
l1vm multiply-test -q >multiply-test-out.txt
test-compare.sh multiply-test.txt multiply-test-out.txt
