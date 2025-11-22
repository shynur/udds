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

message(STATUS "Configuring urpc calculator example...")
file(GLOB CLIENT_SERVER_FEED_TYPES_SOURCES_CXX "*.cxx")
file(GLOB CLIENT_SERVER_FEED_TYPES_SOURCES_IPP "*.ipp")
file(GLOB CLIENT_SERVER_FEED_SOURCES_CPP "*.cpp")

add_executable(urpc
    ${CLIENT_SERVER_FEED_TYPES_SOURCES_CXX}
    ${CLIENT_SERVER_FEED_TYPES_SOURCES_IPP}
    ${CLIENT_SERVER_FEED_SOURCES_CPP}
)
target_link_libraries(urpc fastdds fastcdr)
