cmake_minimum_required(VERSION 3.16)
project(AtomicAndPhysicalConstantsCLib CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The library: hand-written logic (src/aapc.cpp) over generated data headers
# (include/aapc/*.generated.h, produced by codegen/generate.jl).
add_library(aapc STATIC src/aapc.cpp)
target_include_directories(aapc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# A shared build is handy for FFI consumers (e.g. Julia).
add_library(aapc_shared SHARED src/aapc.cpp)
set_target_properties(aapc_shared PROPERTIES OUTPUT_NAME aapc)
target_include_directories(aapc_shared PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

option(AAPC_BUILD_TESTS "Build AtomicAndPhysicalConstantsCLib tests" ON)
if(AAPC_BUILD_TESTS)
  enable_testing()
  add_executable(test_aapc tests/test_aapc.cpp)
  target_link_libraries(test_aapc PRIVATE aapc)
  add_test(NAME test_aapc COMMAND test_aapc)
endif()
