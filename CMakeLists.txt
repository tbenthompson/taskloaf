################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(BUILD_TYPE Release)
include(ExternalProject)

set(TASKLOAF_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(TaskloafBase)

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
    src/taskloaf/run.cpp
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
    ${TASKLOAF_INCLUDE_DIR})
add_dependencies(taskloaf actor-framework)
target_link_libraries(taskloaf ${CAF_LIBRARIES})

################################################################################
#                              taskloaf tests
################################################################################

add_executable(run_tests 
    tests/test_main.cpp
    tests/test_future.cpp
    tests/test_fnc.cpp
    tests/test_data.cpp
    tests/test_run.cpp
    tests/test_worker.cpp
    tests/test_id.cpp
    tests/test_ring.cpp
    tests/test_comm.cpp
    )
target_include_directories(run_tests PRIVATE
    ${CATCH_INCLUDE_DIRS}
    ${TASKLOAF_INCLUDE_DIR}
    ${TASKLOAF_SOURCE_DIR}
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
    target_include_directories(${name} PRIVATE ${TASKLOAF_INCLUDE_DIR})
    add_dependencies(${name} taskloaf)
    target_link_libraries(${name} taskloaf)
endmacro()

taskloaf_example(fib examples/fib.cpp)
taskloaf_example(totient examples/totient.cpp)
taskloaf_example(dot_product examples/dot_product.cpp)
taskloaf_example(cholesky examples/cholesky.cpp)
target_link_libraries(cholesky openblas)
taskloaf_example(diffusion examples/diffusion.cpp)
