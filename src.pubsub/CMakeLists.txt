cmake_minimum_required(VERSION 3.20)
project(UddsExample CXX)

if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()

execute_process(
    COMMAND bash -c "fastddsgen *.idl"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
file(GLOB PROTO_SOURCES "*.cxx")
add_compile_options(-std=c++20 -Wpedantic -Wall -W)

add_executable(pub
    pub.cpp
    ${PROTO_SOURCES}
)
target_link_libraries(pub fastdds fastcdr)

add_executable(sub
    sub.cpp
    ${PROTO_SOURCES}
)
target_link_libraries(sub fastdds fastcdr)
