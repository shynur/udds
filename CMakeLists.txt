cmake_minimum_required(VERSION 3.20)
project(UddsExample)



# 尽可能使用静态链接.
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")



if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB UddsExample_SOURCES_CXX "protos/*.cxx")



add_compile_options(-Wpedantic -Wall -W)



add_executable(send src/example.use-udds-header-file/send.cpp ${UddsExample_SOURCES_CXX})
target_compile_features(send PRIVATE cxx_std_23)  # latest C++ standard
target_link_libraries(send fastdds fastcdr tbb)
add_executable(receive src/example.use-udds-header-file/receive.cpp ${UddsExample_SOURCES_CXX})
target_compile_features(receive PRIVATE cxx_std_23)  # latest C++ standard
target_link_libraries(receive fastdds fastcdr tbb)

add_executable(udds-broadcast-cli src/udds-broadcast-cli.cpp ${UddsExample_SOURCES_CXX})
target_compile_features(udds-broadcast-cli PRIVATE cxx_std_23)  # latest C++ standard
target_link_libraries(udds-broadcast-cli fastdds fastcdr tbb)
target_link_options(udds-broadcast-cli PRIVATE "-Wl,-rpath,$ORIGIN")
set_target_properties(udds-broadcast-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/udds-broadcast-cli.d"
)

add_executable(easily-send src/example.use-individual-process/easily-send.cpp)
target_compile_features(easily-send PRIVATE cxx_std_14)  # rbk
add_executable(easily-receive src/example.use-individual-process/easily-receive.cpp)
target_compile_features(easily-receive PRIVATE cxx_std_14)  # rbk
