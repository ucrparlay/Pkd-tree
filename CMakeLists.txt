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
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # works

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

  add_compile_options(-g -mcx16 -march=native)
  # add_compile_options(-O3 -mcx16 -march=native)
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
add_subdirectory(external/ParGeo)

# add_compile_definitions(PRINT_CONFIG)
# add_compile_definitions(PRINT_LOGTREE_TIMINGS)
# add_compile_definitions(PRINT_DELETE_TIMINGS)
# add_compile_definitions(PRINT_KDTREE_TIMINGS)

if(DEFINED PARTITION_TYPE)
  if(PARTITION_TYPE STREQUAL "PARTITION_OBJECT_MEDIAN")
    set(PARTITION_TYPE 0)
  elseif(PARTITION_TYPE STREQUAL "PARTITION_SPATIAL_MEDIAN")
    set(PARTITION_TYPE 1)
  else()
    message(FATAL_ERROR "Invalid PARTITION_TYPE=${PARTITION_TYPE}")
  endif()
  add_compile_definitions(PARTITION_TYPE=${PARTITION_TYPE})
endif()

if(DEFINED CLUSTER_SIZE)
  add_compile_definitions(CLUSTER_SIZE=${CLUSTER_SIZE})
endif()
if(DEFINED ERASE_BASE_CASE)
  add_compile_definitions(ERASE_BASE_CASE=${ERASE_BASE_CASE})
endif()
if(DEFINED RANGEQUERY_BASE_CASE)
  add_compile_definitions(RANGEQUERY_BASE_CASE=${RANGEQUERY_BASE_CASE})
endif()
if(DEFINED BOUNDINGBOX_BASE_CASE)
  add_compile_definitions(BOUNDINGBOX_BASE_CASE=${BOUNDINGBOX_BASE_CASE})
endif()
if(DEFINED CO_TOP_BUILD_BASE_CASE)
  add_compile_definitions(CO_TOP_BUILD_BASE_CASE=${CO_TOP_BUILD_BASE_CASE})
endif()
if(DEFINED CO_BOTTOM_BUILD_BASE_CASE)
  add_compile_definitions(CO_BOTTOM_BUILD_BASE_CASE=${CO_BOTTOM_BUILD_BASE_CASE})
endif()
if(DEFINED BHL_BUILD_BASE_CASE)
  add_compile_definitions(BHL_BUILD_BASE_CASE=${BHL_BUILD_BASE_CASE})
endif()

OPTION(ALL_USE_BLOOM "all use bloom" OFF)
if(ALL_USE_BLOOM)
  add_compile_definitions(ALL_USE_BLOOM)
endif()

OPTION(LOGTREE_USE_BLOOM "logtree use bloom" OFF)
if(LOGTREE_USE_BLOOM)
  add_compile_definitions(LOGTREE_USE_BLOOM)
endif()

OPTION(BLOOM_FILTER_BUILD_COPY "logtree filter copy" OFF)
if(BLOOM_FILTER_BUILD_COPY)
  add_compile_definitions(BLOOM_FILTER_BUILD_COPY)
endif()

# set(source_files
# ${PROJECT_SOURCE_DIR}/include/cpdd/kdTreeParallel.hpp
# )

# my kd
add_library(kdTreeParallel INTERFACE)
target_link_libraries(kdTreeParallel INTERFACE parlay)
target_include_directories(kdTreeParallel INTERFACE ${PROJECT_SOURCE_DIR}/include)

include(GNUInstallDirs)
install(TARGETS kdTreeParallel
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(test ${PROJECT_SOURCE_DIR}/tests/test.cpp)
target_link_libraries(test PUBLIC kdTreeParallel)
target_include_directories(test PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(test_pg ${PROJECT_SOURCE_DIR}/tests/test_pg.cpp)
target_link_libraries(kdTreeParallel INTERFACE pargeoLib)
target_link_libraries(test_pg PUBLIC kdTreeParallel pargeoLib parlay)
# target_include_directories(test_pg INTERFACE parlay)
target_link_directories(test_pg PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(test_pg PUBLIC ${PROJECT_SOURCE_DIR}/external/ParGeo/include)
target_include_directories(test_pg PUBLIC ${PROJECT_SOURCE_DIR}/external/ParGeo/external/batchKdTree/utils/external)
target_include_directories(test_pg PUBLIC ${PROJECT_SOURCE_DIR}/external/ParGeo/external/batchKdTree/include/common)

add_executable(test_dkdt ${PROJECT_SOURCE_DIR}/tests/test_dkdt.cpp)
target_link_libraries(test_dkdt PUBLIC kdTreeParallel)
target_link_libraries(kdTreeParallel INTERFACE pargeoLib)
target_include_directories(test_dkdt INTERFACE parlay)

if(JEMA)
  target_link_libraries(test PUBLIC libjemalloc.so)
  target_link_libraries(test_pg PUBLIC libjemalloc.so)
  target_link_libraries(test_dkdt PUBLIC libjemalloc.so)
endif()

add_executable(data_generator ${PROJECT_SOURCE_DIR}/tests/data_generate.cpp)
target_link_libraries(data_generator PUBLIC kdTreeParallel)
target_include_directories(data_generator PUBLIC ${PROJECT_SOURCE_DIR}/include)

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
  add_executable(cgal ${PROJECT_SOURCE_DIR}/tests/cgal.cpp)
  target_link_libraries(cgal PUBLIC CGAL::CGAL CGAL::TBB_support PUBLIC parlay)
  target_include_directories(cgal PUBLIC ${PROJECT_SOURCE_DIR}/include)

  add_executable(checkCorrectParallel ${PROJECT_SOURCE_DIR}/tests/checkCorrectParallel.cpp)
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
