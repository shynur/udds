cmake_minimum_required(VERSION 3.20)
project(UddsExample)



# 尽可能使用静态链接.
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++"
)



if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB UddsExample_SOURCES_CXX "protos/*.cxx")



add_compile_options(-Wpedantic -Wall -W)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")



add_executable(send-receive
    src/send-receive.cpp
    ${UddsExample_SOURCES_CXX}
)
target_compile_features(send-receive PRIVATE cxx_std_23)  # latest C++ standard
target_link_libraries(send-receive fastdds fastcdr tbb)

add_executable(udds-broadcast-cli
    src/udds-broadcast-cli.cpp
    ${UddsExample_SOURCES_CXX}
)
target_compile_features(udds-broadcast-cli PRIVATE cxx_std_23)  # latest C++ standard
target_link_libraries(udds-broadcast-cli fastdds fastcdr tbb)
target_link_options(udds-broadcast-cli PRIVATE "-Wl,-rpath,$ORIGIN")
set_target_properties(udds-broadcast-cli
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/udds-broadcast-cli.d"
)

add_executable(easily-send-receive
    src/easily-send-receive.cpp
)
target_compile_features(easily-send-receive PRIVATE cxx_std_14)  # rbk
