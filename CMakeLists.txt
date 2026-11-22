cmake_minimum_required(VERSION 3.16)

# The version tracks the AtomicAndPhysicalConstants.jl release this library
# mirrors, so `apc` v0.12.0 is the C++ view of APC v0.12.0. Bump it whenever the
# data is regenerated against a new APC release, and tag the commit to match:
# downstream projects pin that tag.
project(AtomicAndPhysicalConstantsCLib VERSION 0.12.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The library: hand-written logic (src/apc.cpp) over generated data headers
# (include/apc/*.generated.h, produced by codegen/generate.jl).
add_library(apc STATIC src/apc.cpp)
target_include_directories(apc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# A shared build is handy for FFI consumers (e.g. Julia).
add_library(apc_shared SHARED src/apc.cpp)
set_target_properties(apc_shared PROPERTIES OUTPUT_NAME apc)
# With MSVC a DLL's import library is <name>.lib, the static library's own
# name, so both targets would write apc.lib: Ninja refuses to build that, and
# the Visual Studio generator lets one overwrite the other. Name the import
# library apc_shared.lib instead; apc.dll keeps its name. MinGW's import
# library (libapc.dll.a) never clashed with libapc.a, so is left as it is.
if(MSVC)
  set_target_properties(apc_shared PROPERTIES ARCHIVE_OUTPUT_NAME apc_shared)
endif()
target_include_directories(apc_shared PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

option(APC_BUILD_TESTS "Build AtomicAndPhysicalConstantsCLib tests" ON)
if(APC_BUILD_TESTS)
  enable_testing()
  add_executable(test_apc tests/test_apc.cpp)
  target_link_libraries(test_apc PRIVATE apc)
  add_test(NAME test_apc COMMAND test_apc)
endif()
