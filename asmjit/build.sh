#!/bin/bash
mkdir build
cd build
cmake ../ -DLINK_OPTIONS="-fPIC"
make -j 4
mv libasmjit.so libasmjit-l1vm.so
