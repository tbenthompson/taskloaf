################################################################################
#                              set output paths
################################################################################

# prohibit in-source builds
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

################################################################################
#                              Check for threading library (pthreads...)
################################################################################

# Currently, find_package(Threads) has a bug in CMake. Should be fixed soon.
# find_package(Threads REQUIRED)
set(CMAKE_THREAD_LIBS_INIT -lpthread)

################################################################################
#                              setup compiler
################################################################################

set(warnings "-Wall -Wextra -Werror -Wno-parentheses -Wno-deprecated-declarations")
set(opt "-O3")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
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

set(CMAKE_THREAD_LIBS_INIT -lpthread)
set(TASKLOAF_BINARY_DIR ${TASKLOAF_DIR}/build)
set(TASKLOAF_INCLUDE_DIR ${TASKLOAF_DIR}/src)
set(TASKLOAF_SOURCE_DIR ${TASKLOAF_DIR}/src)
set(CATCH_INCLUDE_DIRS ${TASKLOAF_BINARY_DIR}/Catch-prefix/src/Catch/single_include)
set(SPDLOG_INCLUDE_DIRS ${TASKLOAF_BINARY_DIR}/spdlog-prefix/src/spdlog/include)
set(CAF_ROOT_DIR ${TASKLOAF_BINARY_DIR}/actor-framework-prefix/src)
set(CAF_INCLUDE_DIRS
    ${CAF_ROOT_DIR}/actor-framework/libcaf_core 
    ${CAF_ROOT_DIR}/actor-framework/libcaf_io)
set(CAF_LIBRARIES 
    ${CAF_ROOT_DIR}/actor-framework-build/lib/libcaf_core.so
    ${CAF_ROOT_DIR}/actor-framework-build/lib/libcaf_io.so)
set(FAKEIT_INCLUDE_DIRS 
    ${TASKLOAF_BINARY_DIR}/FakeIt-prefix/src/FakeIt/single_header/catch)
