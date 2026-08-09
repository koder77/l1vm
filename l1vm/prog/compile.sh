#!/bin/bash
FILE=$1
l1com $FILE
RETVAL=$?
[ $RETVAL -ne 0 ] && exit
l1asm $FILE
RETVAL=$?
[ $RETVAL -ne 0 ] && echo "build failed!!!"
