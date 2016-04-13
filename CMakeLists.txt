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

FIND_PACKAGE(OpenMP REQUIRED)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

################################################################################
#                              Check for threading library (pthreads..., OpenMP)
################################################################################

find_package(MPI REQUIRED)

################################################################################
#                              setup compiler
################################################################################

set(warnings "-Wall -Wextra -Werror -Wno-parentheses -Wno-deprecated-declarations -Wno-unknown-pragmas -Wno-literal-suffix")
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

################################################################################
#                              includes
################################################################################

set(PROJECT_BINARY_DIR ${PROJECT_DIR}/build)
set(PROJECT_INCLUDE_DIR ${PROJECT_DIR}/src)
set(PROJECT_SOURCE_DIR ${PROJECT_DIR}/src)

################################################################################
#                              Grab MPMC queue
################################################################################

ExternalProject_Add(
    concurrentqueue
    DOWNLOAD_COMMAND git clone --depth 1
	git@github.com:cameron314/concurrentqueue.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)
set(MPMCQUEUE_ROOT_DIR ${PROJECT_BINARY_DIR}/concurrentqueue-prefix/src)
set(MPMCQUEUE_INCLUDE_DIR ${MPMCQUEUE_ROOT_DIR}/concurrentqueue)

################################################################################
#                              Grab Cereal for serialization
################################################################################

ExternalProject_Add(
    cereal
    DOWNLOAD_COMMAND git clone --depth 1
        git@github.com:USCiLab/cereal.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)
set(CEREAL_ROOT_DIR ${PROJECT_BINARY_DIR}/cereal-prefix/src)
set(CEREAL_INCLUDE_DIR ${CEREAL_ROOT_DIR}/cereal/include)

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
set(CATCH_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/Catch-prefix/src/Catch/single_include)

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
    src/taskloaf/ref_counting.cpp
    src/taskloaf/local_comm.cpp
    src/taskloaf/serializing_comm.cpp
    src/taskloaf/comm.cpp
    src/taskloaf/launcher.cpp
    src/taskloaf/mpi_comm.cpp
    )

MESSAGE(${CEREAL_INCLUDE_DIR})
target_include_directories(taskloaf PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${MPMCQUEUE_INCLUDE_DIR}
    ${CEREAL_INCLUDE_DIR}
    ${MPI_INCLUDE_PATH}
    )
target_link_libraries(taskloaf ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(taskloaf ${MPI_LIBRARIES})
add_dependencies(taskloaf concurrentqueue cereal)

################################################################################
#                              taskloaf tests
################################################################################

macro(taskloaf_tests name files)
    add_executable(${name} ${files})
    target_include_directories(${name} PRIVATE
        ${PROJECT_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${MPMCQUEUE_INCLUDE_DIR}
        ${CATCH_INCLUDE_DIRS}
        ${CEREAL_INCLUDE_DIR}
        ${MPI_INCLUDE_PATH}
        )
    add_dependencies(${name} taskloaf Catch)
    target_link_libraries(${name} taskloaf)
    target_link_libraries(${name} ${MPI_LIBRARIES})
endmacro()

taskloaf_tests(run_tests
    "tests/test_main.cpp;tests/test_future.cpp;tests/test_fnc.cpp;tests/test_data.cpp;tests/test_worker.cpp;tests/test_id.cpp;tests/test_ring.cpp;tests/test_comm.cpp;tests/test_ref_counting.cpp;tests/test_fakelaunch.cpp"
)

taskloaf_tests(mpi_tests "tests/test_mpi.cpp")

enable_testing()
add_test(all-tests mpi_tests)
add_test(all-tests run_tests)

################################################################################
#                              taskloaf examples
################################################################################

macro(taskloaf_example name filename) 
    add_executable(${name} ${filename})
    target_include_directories(${name} PRIVATE 
        ${PROJECT_INCLUDE_DIR}
        ${MPMCQUEUE_INCLUDE_DIR}
        ${CEREAL_INCLUDE_DIR}
        ${MPI_INCLUDE_PATH}
    )
    add_dependencies(${name} taskloaf)
    target_link_libraries(${name} taskloaf)
endmacro()

taskloaf_example(fib examples/fib.cpp)
taskloaf_example(totient examples/totient.cpp)
taskloaf_example(dot_product examples/dot_product.cpp)
taskloaf_example(cholesky examples/cholesky.cpp)
target_link_libraries(cholesky openblas)
taskloaf_example(diffusion examples/diffusion.cpp)
taskloaf_example(fibmpi examples/fibmpi.cpp)
