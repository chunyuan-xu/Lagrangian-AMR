cmake_minimum_required(VERSION 3.20)

project(p4est_AMR_Hydro LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------- MPI (Microsoft MPI SDK + MinGW gcc) ----------
if(WIN32 AND NOT DEFINED ENV{MSMPI_INC})
  set(_msmpi_inc "$ENV{ProgramFiles\(x86\)}/Microsoft SDKs/MPI/Include")
  cmake_path(NORMAL_PATH _msmpi_inc OUTPUT_VARIABLE _msmpi_inc_norm)
  if(EXISTS "${_msmpi_inc_norm}/mpi.h")
    file(TO_NATIVE_PATH "${_msmpi_inc_norm}" _msmpi_inc_native)
    set(ENV{MSMPI_INC} "${_msmpi_inc_native}")
    set(_msmpi_lib64 "$ENV{ProgramFiles\(x86\)}/Microsoft SDKs/MPI/Lib/x64")
    cmake_path(NORMAL_PATH _msmpi_lib64 OUTPUT_VARIABLE _msmpi_lib64_norm)
    file(TO_NATIVE_PATH "${_msmpi_lib64_norm}" _msmpi_lib64_native)
    set(ENV{MSMPI_LIB64} "${_msmpi_lib64_native}")
    set(MPI_GUESS_LIBRARY_NAME "MSMPI" CACHE STRING "" FORCE)
  endif()
endif()

find_package(MPI REQUIRED COMPONENTS C CXX)

# ---------- p4est / libsc ----------
option(AMR_FETCH_P4EST "Fetch p4est from GitHub or use third_party/p4est" ON)
set(AMR_P4EST_ROOT "" CACHE PATH "Install prefix for libsc+p4est (when AMR_FETCH_P4EST=OFF)")

macro(_amr_p4est_cache_options)
  set(mpi ON CACHE BOOL "MPI for libsc and p4est" FORCE)
  # zlib=ON pulls zlib-ng from GitHub inside libsc (cmake/zlib.cmake); keep OFF on Windows unless you patch that.
  # third_party/p4est/sc/src/sc_io.c is patched so zlib=OFF still builds.
  set(zlib ON CACHE BOOL "zlib in libsc (avoid ExternalProject zlib-ng)" FORCE)
  set(enable_p8est OFF CACHE BOOL "Build p8est (3D)" FORCE)
  set(enable_p6est OFF CACHE BOOL "Build p6est" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "libsc tests" FORCE)
endmacro()

if(AMR_FETCH_P4EST)
  _amr_p4est_cache_options()

  set(_tp_p4est "${CMAKE_CURRENT_SOURCE_DIR}/third_party/p4est")
  set(_tp_sc "${_tp_p4est}/sc")

  if(EXISTS "${_tp_p4est}/CMakeLists.txt" AND EXISTS "${_tp_sc}/CMakeLists.txt")
    add_subdirectory("${_tp_sc}")
    add_subdirectory("${_tp_p4est}")
  elseif(EXISTS "${_tp_p4est}/CMakeLists.txt")
    message(FATAL_ERROR
      "third_party/p4est exists but sc/ is incomplete.\n"
      "  cd third_party/p4est && git submodule update --init\n"
      "Or: git clone --recursive ... third_party/p4est")
  else()
    include(FetchContent)
    FetchContent_Declare(
      p4est
      GIT_REPOSITORY https://github.com/cburstedde/p4est.git
      GIT_TAG v2.8.5
      GIT_SUBMODULES sc
    )
    FetchContent_MakeAvailable(p4est)
  endif()
else()
  if(NOT AMR_P4EST_ROOT)
    message(FATAL_ERROR "Set AMR_P4EST_ROOT to an install prefix containing libsc+p4est CMake packages, or use AMR_FETCH_P4EST=ON.")
  endif()
  list(APPEND CMAKE_PREFIX_PATH "${AMR_P4EST_ROOT}")
  find_package(SC REQUIRED CONFIG PATHS "${AMR_P4EST_ROOT}" NO_DEFAULT_PATH)
  find_package(P4EST REQUIRED CONFIG PATHS "${AMR_P4EST_ROOT}" NO_DEFAULT_PATH)
endif()

# ---------- AMR executable ----------
add_executable(AMR_Solver
  src/main.cpp
  src/alg.cpp
  src/io/config_parser.cpp
)

target_include_directories(AMR_Solver PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(AMR_Solver PRIVATE
  P4EST::P4EST
  MPI::MPI_CXX
  MPI::MPI_C
)

if(UNIX OR CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR MINGW)
  target_link_libraries(AMR_Solver PRIVATE m)
endif()

set_target_properties(AMR_Solver PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
