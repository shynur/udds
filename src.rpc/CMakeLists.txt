cmake_minimum_required(VERSION 3.20)
project(urpc_example LANGUAGES CXX)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/fastdds")
if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()
if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()

execute_process(
    COMMAND bash -c "PATH+=:/opt/fastdds/bin; fastddsgen -replace *.idl"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB FASTDDS_GEN_CXX "*.cxx")
file(GLOB FASTDDS_GEN_IPP "*.ipp")
file(GLOB LIBS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM LIBS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/a.cpp")


add_library(urpc_autogen SHARED ${FASTDDS_GEN_IPP} ${FASTDDS_GEN_CXX})
target_link_libraries(urpc_autogen PRIVATE libfastdds.so libfastcdr.so)

set(SO_LIBS "")
foreach(LIB_CPP ${LIBS_CPP})
    get_filename_component(LIB_NAME ${LIB_CPP} NAME_WE)
    add_library(${LIB_NAME} SHARED ${LIB_CPP})
    target_link_libraries(${LIB_NAME} PRIVATE urpc_autogen)
    list(APPEND SO_LIBS ${LIB_NAME})
endforeach()

add_executable(a.out "${CMAKE_CURRENT_SOURCE_DIR}/a.cpp")
target_link_libraries(a.out PRIVATE ${SO_LIBS})
