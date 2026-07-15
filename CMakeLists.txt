cmake_minimum_required(VERSION 3.16)
project(AtomicAndPhysicalConstantsCLib CXX)

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
target_include_directories(apc_shared PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

option(APC_BUILD_TESTS "Build AtomicAndPhysicalConstantsCLib tests" ON)
if(APC_BUILD_TESTS)
  enable_testing()
  add_executable(test_apc tests/test_apc.cpp)
  target_link_libraries(test_apc PRIVATE apc)
  add_test(NAME test_apc COMMAND test_apc)
endif()
