cmake_minimum_required(VERSION 3.18)
project(LagrangianAMR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(MPI REQUIRED COMPONENTS CXX)
find_package(ZLIB REQUIRED)

set(P4EST_ROOT "" CACHE PATH
    "p4est install prefix containing include/ and lib/")

find_path(P4EST_INCLUDE_DIR p4est.h
    HINTS "${P4EST_ROOT}/include"
    REQUIRED)
find_path(SC_INCLUDE_DIR sc.h
    HINTS "${P4EST_ROOT}/include"
    REQUIRED)
find_library(P4EST_LIBRARY NAMES p4est
    HINTS "${P4EST_ROOT}/lib"
    REQUIRED)
find_library(SC_LIBRARY NAMES sc
    HINTS "${P4EST_ROOT}/lib"
    REQUIRED)

add_executable(AMR_Solver
    src/main.cpp
    src/alg.cpp
    src/io/config_parser.cpp)

target_include_directories(AMR_Solver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${P4EST_INCLUDE_DIR}
    ${SC_INCLUDE_DIR})

target_link_libraries(AMR_Solver PRIVATE
    MPI::MPI_CXX
    ${P4EST_LIBRARY}
    ${SC_LIBRARY}
    ZLIB::ZLIB)

if(UNIX)
    target_link_libraries(AMR_Solver PRIVATE m)
endif()

if(WIN32)
    target_link_libraries(AMR_Solver PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_definitions(AMR_Solver PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
