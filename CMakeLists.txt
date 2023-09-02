cmake_minimum_required(VERSION 3.15)

project(
  kdTree
  VERSION 1.0
  DESCRIPTION "KD tree"
  HOMEPAGE_URL ""
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

option(DEBUG "" ON)
option(CILKPLUS "" OFF)
option(OPENCILK "" OFF)
option(SERIAL "" OFF)
option(STDALLOC "" OFF)
option(CGAL "" ON)
option(MEMCHECK "" OFF)
option(JEMA "" OFF)

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

  # add_compile_options(-g -mcx16 -march=native)
  add_compile_options(-O3 -mcx16 -march=native)
elseif(MEMCHECK)
  set(CMAKE_BUILD_TYPE "Debug")
  add_compile_options(-Og -mcx16 -DPARLAY_SEQUENTIAL)
else()
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(-O3 -mcx16 -march=native)
endif()

if(STDALLOC)
  add_compile_options(-DPARLAY_USE_STD_ALLOC)
elseif(JEMA)
  include_directories("/usr/local/lib")
  set(CMAKE_BUILD_RPATH "/usr/local/lib")
endif()

add_subdirectory(parlaylib)

set(source_files

  # ${PROJECT_SOURCE_DIR}/src/kdTree.cpp
  ${PROJECT_SOURCE_DIR}/src/kdTreeParallel.cpp
)

# my kd
# add_library(kdTree INTERFACE)
# target_include_directories(kdTree INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
# target_link_libraries(kdTree INTERFACE kdTreeParallel)
add_library(kdTreeParallel ${source_files})
target_link_libraries(kdTreeParallel PUBLIC parlay)

add_executable(test ../src/test.cpp)
target_link_libraries(test PUBLIC kdTreeParallel)

if(JEMA)
  target_link_libraries(test PUBLIC libjemalloc.so)
endif()

add_executable(data_generator ../src/data_generate.cpp)
target_link_libraries(data_generator PUBLIC kdTreeParallel)

# parlay::zd tree
# add_executable(zdtree ../src_x/zdtree.cpp)
# target_link_libraries(zdtree PUBLIC parlay)

# CGAL
if(CGAL)
  find_package(CGAL REQUIRED)

  # TBB
  find_package(TBB)

  # copied from CGAL default cmake
  if(TBB_FOUND AND NOT TARGET CGAL::TBB_support)
    message("here")

    if(NOT TARGET Threads::Threads)
      find_package(Threads REQUIRED)
    endif()

    add_library(CGAL::TBB_support INTERFACE IMPORTED)
    set_target_properties(CGAL::TBB_support PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS "CGAL_LINKED_WITH_TBB;NOMINMAX"
      INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "TBB::tbb;TBB::tbbmalloc;Threads::Threads")
  endif()

  list(APPEND CGAL_3RD_PARTY_LIBRARIES ${TBB_LIBRARIES})

  # executables
  add_executable(cgal "../src_x/cgal.cpp")
  target_link_libraries(cgal PUBLIC CGAL::CGAL CGAL::TBB_support)
  target_include_directories(cgal INTERFACE kdTreeParallel)

  # add_executable(checkCorrect ../src/checkCorrect.cpp)
  # target_link_libraries(checkCorrect PUBLIC kdTreeParallel CGAL::CGAL CGAL::TBB_support)
  add_executable(checkCorrectParallel ../src/checkCorrectParallel.cpp)
  target_link_libraries(checkCorrectParallel PUBLIC kdTreeParallel CGAL::CGAL CGAL::TBB_support)
endif()

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