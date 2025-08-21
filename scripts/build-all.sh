#!/bin/bash
# build demo programs

<<<<<<< HEAD
cd ~/l1vm
=======
cd ..
>>>>>>> f441381d19b3e7f4cfcfbc6d6b00520dd63d357e

for i in prog/*.l1asm
do
	filename=$(basename "$i" .l1asm)
	l1asm prog/$filename
done
	
for i in prog/*.l1com
do
	filename=$(basename "$i" .l1com)
	sh ./l1vm-build.sh prog/$filename -sizes 1000000 1000000000
done
rm prog/*.l1asm.l1dbg
exit 0
