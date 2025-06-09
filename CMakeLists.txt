cmake_minimum_required(VERSION 3.20)
project(UddsExample)



if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB UddsExample_SOURCES_CXX "protos/*.cxx")



add_executable(send src/send.cpp ${UddsExample_SOURCES_CXX})
target_link_libraries(send fastdds fastcdr tbb)
add_executable(receive src/receive.cpp ${UddsExample_SOURCES_CXX})
target_link_libraries(receive fastdds fastcdr tbb)

add_executable(udds-broadcast-cli src/udds-broadcast-cli.cpp ${UddsExample_SOURCES_CXX})
target_link_libraries(udds-broadcast-cli fastdds fastcdr tbb)

add_executable(udds-broadcast-client-send src/udds-broadcast-client-send.cpp)
target_compile_features(udds-broadcast-client-send PRIVATE cxx_std_14)
add_executable(udds-broadcast-client-receive src/udds-broadcast-client-receive.cpp)
target_compile_features(udds-broadcast-client-receive PRIVATE cxx_std_14)
