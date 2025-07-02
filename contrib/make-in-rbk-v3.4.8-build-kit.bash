#! /bin/bash

set -e

cd `dirname $0`/..

make clean

clear

CC=g++-15 CXX=g++-15 CFLAGS= CXXFLAGS= make

make install

echo '开始编译 bin/easily-send-receive...'
clang++-6.0 -std=c++14 -Wno-c++17-extensions  \
    -O0 -g3 -ggdb -glldb -I./include  \
    -static-libgcc -static-libstdc++  \
    -D'SHYNUR_UDDS_USED_BY_SEER_RBK=30408UL'  \
    src/easily-send-receive.cpp  \
    -o bin/easily-send-receive
echo '编译完成: bin/easily-send-receive'
sudo bash -c  \
    'rm -f /bin/easily-send-receive; ln -s `pwd`/bin/easily-send-receive /bin/easily-send-receive'
