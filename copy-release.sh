#!/bin/bash
mkdir bin-release
cp modules/*.so bin-release
cp assemb/l1asm* bin-release
cp comp/l1com* bin-release
cp disasm/l1vm-disasm* bin-release
cp prepro/l1pre* bin-release
cp linter/l1vm-linter* bin-release
cp vm/l1vm* bin-release
cp vm/l1vm-nojit* bin-release
cp zerobuild/zerobuild*  bin-release
