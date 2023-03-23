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
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -w")

option(DEBUG "" ON)
option(CILKPLUS "" OFF)
option(OPENCILK "" OFF)
option(SERIAL "" OFF)
option(STDALLOC "" OFF)
option(CGAL "" OFF)

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

add_executable(test ../src/test.cpp)
target_link_libraries(test PUBLIC  kdTreeParallel)

add_library(kdTreeParallel ../src/kdTreeParallel.cpp)
target_link_libraries(kdTreeParallel PUBLIC parlay)

add_executable(data_generator ../src/data_generate.cpp)
target_link_libraries(data_generator PUBLIC kdTreeParallel)

#CGAL 
find_package( CGAL )
if ( NOT CGAL_FOUND )
  message(STATUS "This project requires the CGAL library, and will not be compiled.")
  return()  
endif()

add_executable(cgal "../src_x/cgal_knn.cpp")
target_link_libraries(cgal CGAL::CGAL)
# target_compile_options(cgal_knn PRIVATE -w)

FIND_PROGRAM(CLANG_FORMAT "clang-format")
IF(CLANG_FORMAT)
    add_custom_target(
        format
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${CLANG_FORMAT}
        -style=file
        -i
        ${all_files}
    )
endif()