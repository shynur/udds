#! /bin/bash

set -e

cd `dirname $0`/..

make clean

clear

CC='clang-20 --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.1.0'  \
CXX='clang++-20 --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.1.0'  \
CFLAGS+=' -g3' CXXFLAGS+=' -g3'  \
make

make install

mkdir -p bin
clang++-6.0 -std=c++14 -O0 -g3 -I./include  \
    -static-libgcc -static-libstdc++  \
    src/easily-send-receive.cpp  \
    -o bin/easily-send-receive
