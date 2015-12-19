################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_BUILD_TYPE Release)
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
#                              Grab spdlog
################################################################################

ExternalProject_Add(
    spdlog 
    DOWNLOAD_COMMAND git clone --depth 1 git@github.com:gabime/spdlog.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
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
    src/start.cpp
    src/run.cpp
    src/id.cpp
    )

target_include_directories(taskloaf PRIVATE
    ${CAF_INCLUDE_DIRS} 
    ${SPDLOG_INCLUDE_DIRS}
    ${TASKLOAF_INCLUDE_DIR})
add_dependencies(taskloaf actor-framework spdlog )
target_link_libraries(taskloaf ${CAF_LIBRARIES})


################################################################################
#                              taskloaf tests
################################################################################

add_executable(run_tests 
    tests/test_main.cpp
    tests/test_future.cpp
    tests/test_fnc.cpp
    tests/test_data.cpp
    tests/test_print.cpp
    tests/test_run.cpp
    tests/test_id.cpp
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
