################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(BUILD_TYPE Release)
include(ExternalProject)


set(OPENMP_REQUIRED true)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

################################################################################
#                              set output paths
################################################################################

# prohibit in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

################################################################################
#                              Check for threading library (pthreads..., OpenMP)
################################################################################

# Currently, find_package(Threads) has a bug in CMake. Should be fixed soon.
# find_package(Threads REQUIRED)
set(CMAKE_THREAD_LIBS_INIT -lpthread)

FIND_PACKAGE(OpenMP REQUIRED)
if(OPENMP_FOUND)
    message("OPENMP FOUND")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

################################################################################
#                              setup compiler
################################################################################

set(warnings "-Wall -Wextra -Werror -Wno-parentheses -Wno-deprecated-declarations")
set(opt "-O3 -g") 
if (BUILD_TYPE STREQUAL "Debug")
    set(opt "-O0 -g -DSPDLOG_TRACE_ON")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warnings} ${opt}")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support.
                    Please use a different C++ compiler.")
endif() 
MESSAGE(${CMAKE_CXX_FLAGS})

################################################################################
#                              includes
################################################################################

set(CMAKE_THREAD_LIBS_INIT -lpthread)
set(PROJECT_BINARY_DIR ${PROJECT_DIR}/build)
set(PROJECT_INCLUDE_DIR ${PROJECT_DIR}/src)
set(PROJECT_SOURCE_DIR ${PROJECT_DIR}/src)

set(CATCH_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/Catch-prefix/src/Catch/single_include)
set(CAF_ROOT_DIR ${PROJECT_BINARY_DIR}/actor-framework-prefix/src)
set(CAF_INCLUDE_DIRS
    ${CAF_ROOT_DIR}/actor-framework/libcaf_core 
    ${CAF_ROOT_DIR}/actor-framework/libcaf_io)
set(CAF_LIBRARIES 
    ${CAF_ROOT_DIR}/actor-framework-build/lib/libcaf_core.so
    ${CAF_ROOT_DIR}/actor-framework-build/lib/libcaf_io.so)

################################################################################
#                              Grab C++ Actor Framework
################################################################################

ExternalProject_Add(
    actor-framework
    DOWNLOAD_COMMAND git clone --depth 1
        git@github.com:actor-framework/actor-framework.git
    CONFIGURE_COMMAND <SOURCE_DIR>/configure 
        --no-examples --no-riac --no-cash --no-nexus --no-opencl --no-unit-tests
        --build-dir=<BINARY_DIR>
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

################################################################################
#                              Grab Catch
################################################################################

ExternalProject_Add(
    Catch 
    DOWNLOAD_COMMAND git clone --depth 1 git@github.com:philsquared/Catch.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

################################################################################
#                              taskloaf library
################################################################################

add_library(taskloaf SHARED
    src/taskloaf/future.cpp
    src/taskloaf/ivar.cpp
    src/taskloaf/worker.cpp
    src/taskloaf/task_collection.cpp
    src/taskloaf/ivar_tracker.cpp
    src/taskloaf/id.cpp
    src/taskloaf/ring.cpp
    src/taskloaf/caf_comm.cpp
    )

target_include_directories(taskloaf PRIVATE
    ${CAF_INCLUDE_DIRS} 
    ${PROJECT_INCLUDE_DIR})
add_dependencies(taskloaf actor-framework)
target_link_libraries(taskloaf ${CAF_LIBRARIES} -lboost_system)

################################################################################
#                              taskloaf tests
################################################################################

add_executable(run_tests 
    tests/test_main.cpp
    tests/test_future.cpp
    tests/test_fnc.cpp
    tests/test_data.cpp
    tests/test_worker.cpp
    tests/test_id.cpp
    tests/test_ring.cpp
    tests/test_comm.cpp
    )
target_include_directories(run_tests PRIVATE
    ${CATCH_INCLUDE_DIRS}
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}
    )
add_dependencies(run_tests taskloaf Catch)
target_link_libraries(run_tests taskloaf)
target_link_libraries(taskloaf ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_test(all-tests run_tests)

################################################################################
#                              taskloaf examples
################################################################################

macro(taskloaf_example name filename) 
    add_executable(${name} ${filename})
    target_include_directories(${name} PRIVATE ${PROJECT_INCLUDE_DIR})
    add_dependencies(${name} taskloaf)
    target_link_libraries(${name} taskloaf)
endmacro()

taskloaf_example(fib examples/fib.cpp)
taskloaf_example(totient examples/totient.cpp)
taskloaf_example(dot_product examples/dot_product.cpp)
taskloaf_example(cholesky examples/cholesky.cpp)
target_link_libraries(cholesky openblas)
taskloaf_example(diffusion examples/diffusion.cpp)
