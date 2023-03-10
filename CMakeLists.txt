cmake_minimum_required(VERSION 3.15)

project(
  kdTree
  VERSION 1.0
  DESCRIPTION "KD tree"
  HOMEPAGE_URL ""
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(DEBUG "" ON)
option(CILKPLUS "" OFF)
option(OPENCILK "" OFF)
option(SERIAL "" OFF)
option(STDALLOC "" OFF)

if(CILKPLUS)
  add_compile_options(-DPARLAY_CILKPLUS -DCILK -fcilkplus)
elseif(OPENCILK)
  add_compile_options(-DPARLAY_OPENCILK -DCILK -fopencilk)
elseif(SERIAL)
  add_compile_options(-DPARLAY_SEQUENTIAL)
else()
  add_compile_options(-pthread)
endif()

if(DEBUG)
  set(CMAKE_BUILD_TYPE "Debug")
  add_compile_options(-O3 -mcx16 -march=native)
else()
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(-O3 -mcx16 -march=native)
endif()

if(STDALLOC)
  add_compile_options(-DPARLAY_USE_STD_ALLOC)
endif()

add_subdirectory(parlaylib)

add_library(kdTree INTERFACE)
target_include_directories(kdTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdTree INTERFACE kdTreeParallel)

add_executable(test test.cpp)
target_link_libraries(test PUBLIC  kdTreeParallel)
target_compile_options(test PRIVATE -w)

add_library(kdTreeParallel kdTreeParallel.cpp)
target_link_libraries(kdTreeParallel PUBLIC parlay)

add_executable(data_generator data_generate.cpp)
target_link_libraries(data_generator PUBLIC kdTreeParallel)
target_compile_options(data_generator PRIVATE -w)