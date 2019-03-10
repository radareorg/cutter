#!/bin/bash

git clone https://github.com/google/breakpad.git
cd breakpad
git clone https://chromium.googlesource.com/linux-syscall-support src/third_party/lss
./configure --prefix=`pwd`/prefix && make && make install

export CUSTOM_BREAKPAD_PREFIX="`pwd`/prefix"
cd ..