cmake_minimum_required(VERSION 3.20)

project(UddsExample)

if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()
if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()

include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG OR
        CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag(-std=c++23 SUPPORTS_CXX23)
    if(SUPPORTS_CXX23)
        add_compile_options(-std=c++23)
    else()
        message(FATAL_ERROR "Compiler doesn't support C++23")
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB UddsExample_SOURCES_CXX "protos/*.cxx")

add_executable(send src/send.cpp ${UddsExample_SOURCES_CXX})
target_link_libraries(send fastdds fastcdr)
add_executable(receive src/receive.cpp ${UddsExample_SOURCES_CXX})
target_link_libraries(receive fastdds fastcdr)
