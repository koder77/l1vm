#!/bin/bash
if [ "$#" -lt 1 ]
then
  echo "new-program.sh <program-name>"
  exit 1
fi
echo "#include <intr-func.l1h>" > $1.l1com
echo "" >> $1.l1com
echo "(main func)" >> $1.l1com
echo "	(set int64 1 zero 0)" >> $1.l1com
echo "	(set string s hello \"Hello world!\")" >> $1.l1com
echo "	(hello :print_s !)" >> $1.l1com
echo "	(:print_n !)" >> $1.l1com
echo "	(zero :exit !)" >> $1.l1com
echo "(funcend)" >> $1.l1com
exit 0
