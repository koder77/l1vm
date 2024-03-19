#!/bin/bash
mkdir build
cd build
cmake ../
make -j 4
mv libasmjit.so libasmjit-l1vm.so
