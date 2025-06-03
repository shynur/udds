#! /bin/bash

# Usage: $0

cd `dirname $0`/..

CXXFLAGS+=' -std=c++26' make

(
    cd include
    for f in *.hpp; do
        echo '#define SHYNUR_UDDS_USED_BY_SEER_RBK 30408UL' > ../$f
        cat $f >> ../$f
    done
)

for f in protos/*; do
    mv $f .
done

cat <<'EOF' > CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
set(RBK_LIBRARY "RBK_LIBRARY")
set(PROJECT_NAME utils_udds)
if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()
if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()
include(SetOutputDirectory)
include(utils)
add_definitions("-D${RBK_SHARED_LIB}")
ALLCODEFILELIST(codefilelist ${CMAKE_CURRENT_SOURCE_DIR} "")
add_library(${PROJECT_NAME} SHARED ${codefilelist})
add_dependencies(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} fastdds fastcdr)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "rbk/utils")
EOF
