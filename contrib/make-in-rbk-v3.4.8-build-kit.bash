#! /bin/bash

set -e

cd `dirname $0`/..

make clean

clear

CC='clang-20 --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.1.0'  \
CXX='clang++-20 --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.1.0'  \
CFLAGS+=' -g3' CXXFLAGS+=' -g3'  \
make
