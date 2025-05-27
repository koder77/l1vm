#!/bin/bash
mkdir build
cd build
export WIN32=1
cmake ../
make -j 4
mv libasmjit.so libasmjit-l1vm.so

