cmake_minimum_required(VERSION 3.20)
project(UddsExample CXX)



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



add_compile_options(
    -Wpedantic -Wall -W
)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options("$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g3>"
    "$<$<CONFIG:Debug>:-ggdb>"
    "$<$<CONFIG:Debug>:-gpubnames>"
    "$<$<CONFIG:Debug>:-gcolumn-info>"
    "$<$<CONFIG:Debug>:-ggnu-pubnames>"
    "$<$<CONFIG:Debug>:-gno-strict-dwarf>"
    "$<$<CONFIG:Debug>:-grecord-gcc-switches>"
    "$<$<CONFIG:Debug>:-fno-eliminate-unused-debug-types>"
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        "$<$<CONFIG:Debug>:-fvar-tracking>"
        "$<$<CONFIG:Debug>:-gno-prune-btf>"
        "$<$<CONFIG:Debug>:-gdescribe-dies>"
        "$<$<CONFIG:Debug>:-ginline-points>"
        "$<$<CONFIG:Debug>:-fdebug-types-section>"
        "$<$<CONFIG:Debug>:-gstatement-frontiers>"
        "$<$<CONFIG:Debug>:-fno-merge-debug-strings>"
        "$<$<CONFIG:Debug>:-femit-class-debug-always>"
        "$<$<CONFIG:Debug>:-fvar-tracking-assignments>"
        "$<$<CONFIG:Debug>:-fno-eliminate-unused-debug-symbols>"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        "$<$<CONFIG:Debug>:-gdbx>"
        "$<$<CONFIG:Debug>:-gsce>"
        "$<$<CONFIG:Debug>:-gfull>"
        "$<$<CONFIG:Debug>:-glldb>"
        "$<$<CONFIG:Debug>:-gmodules>"
        "$<$<CONFIG:Debug>:-fdebug-macro>"
        "$<$<CONFIG:Debug>:-gembed-source>"
        "$<$<CONFIG:Debug>:-gtemplate-alias>"
        "$<$<CONFIG:Debug>:-fstandalone-debug>"
        "$<$<CONFIG:Debug>:-ginline-line-tables>"
        "$<$<CONFIG:Debug>:-gno-omit-unreferenced-methods>"
    )
endif()

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
