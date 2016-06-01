################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX C)
include(ExternalProject)

option(CMAKE_BUILD_TYPE "Build type" Debug)
option(BUILD_LIBRARY "Build taskloaf library" ON)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_PYTHON "Build python wrappers" OFF)
option(PYTHON_INCLUDE "Python include directories" "")


################################################################################
#                              setup compiler
################################################################################

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support.
                    Please use a different C++ compiler.")
endif() 

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.6)
        set(CMAKE_CXX_FLAGS_DEBUG "-O0")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-mismatched-tags")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-literal-suffix")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

find_package(Threads REQUIRED)

# Turn on taskloaf assertions
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DTLASSERT)
endif()

################################################################################
#                              Check for MPI!
################################################################################

find_package(MPI)
if (MPI_CXX_FOUND)
    add_definitions(-DMPI_FOUND)
endif()

################################################################################
#                              prohibit in-source builds
################################################################################

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

################################################################################
#                              taskloaf library
################################################################################

set(TASKLOAF_SRC "src/taskloaf/address.cpp;src/taskloaf/global_ref.cpp;src/taskloaf/default_worker.cpp;src/taskloaf/worker.cpp;src/taskloaf/default_task_collection.cpp;src/taskloaf/ring_ref_tracker.cpp;src/taskloaf/id.cpp;src/taskloaf/ring.cpp;src/taskloaf/ref_counting.cpp;src/taskloaf/local_comm.cpp;src/taskloaf/serializing_comm.cpp;src/taskloaf/comm.cpp;src/taskloaf/launcher.cpp;src/taskloaf/mpi_comm.cpp;src/taskloaf/tlassert.cpp;src/taskloaf/fnc.cpp;src/taskloaf/data.cpp")
file(GLOB TASKLOAF_HDR "src/taskloaf/*")

if (BUILD_LIBRARY)
    add_library(taskloaf SHARED ${TASKLOAF_SRC})

    target_include_directories(taskloaf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        )

    target_link_libraries(taskloaf ${CMAKE_THREAD_LIBS_INIT})

    if (MPI_CXX_FOUND)
        target_include_directories(taskloaf SYSTEM PRIVATE ${MPI_INCLUDE_PATH})
        target_link_libraries(taskloaf ${MPI_LIBRARIES})
    endif()
    install(TARGETS taskloaf DESTINATION lib)
    install(FILES ${TASKLOAF_HDR} DESTINATION include)
endif(BUILD_LIBRARY)

################################################################################
#                              taskloaf tests
################################################################################

macro(taskloaf_tests name ADDTEST)
    add_executable(${name} "$<TARGET_OBJECTS:test_runner>;tests/test_${name}.cpp")
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib)

    target_include_directories(${name} SYSTEM PRIVATE
        ${MPI_INCLUDE_PATH})
    target_link_libraries(${name} 
        taskloaf
        ${MPI_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
    if (${ADDTEST}) 
        add_test(${name} ${name})
    endif()
    add_dependencies(tests ${name})
endmacro()

if(BUILD_TESTS)
    add_library(test_runner OBJECT tests/test_main.cpp)
    target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    enable_testing()

    add_custom_target(tests)
    taskloaf_tests(task_locations True)
    taskloaf_tests(future True)
    taskloaf_tests(fnc True)
    taskloaf_tests(data True)
    taskloaf_tests(worker True)
    taskloaf_tests(id True)
    taskloaf_tests(ring True)
    taskloaf_tests(comm True)
    taskloaf_tests(ref_counting True)
    taskloaf_tests(launch True)
    taskloaf_tests(gref True)
    taskloaf_tests(task_collection True)

    if (MPI_CXX_FOUND)
        taskloaf_tests(mpi False)
        add_test(mpi mpirun -n 2 mpi)
    endif()
endif(BUILD_TESTS)

################################################################################
#                              taskloaf python bindings
################################################################################

if (BUILD_PYTHON)
    set(WRAPPER_SRC "${TASKLOAF_SRC};taskloaf/taskloaf_wrapper.cpp")
    add_library(taskloaf_wrapper SHARED ${WRAPPER_SRC})
    SET_TARGET_PROPERTIES(taskloaf_wrapper PROPERTIES PREFIX "")

    target_include_directories(taskloaf_wrapper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    target_include_directories(taskloaf_wrapper SYSTEM PRIVATE
        ${PYTHON_INCLUDE}
        ${MPI_INCLUDE_PATH})

    target_link_libraries(taskloaf_wrapper
        ${MPI_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
endif(BUILD_PYTHON)

################################################################################
#                              taskloaf examples
################################################################################

macro(taskloaf_example name filename) 
    add_executable(${name} ${filename})
    target_include_directories(${name} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
    )
    if (MPI_CXX_FOUND)
        target_include_directories(${name} SYSTEM PRIVATE ${MPI_INCLUDE_PATH})
    endif()
    target_link_libraries(${name} 
        taskloaf
        ${CMAKE_THREAD_LIBS_INIT})
endmacro()

if (BUILD_EXAMPLES)
    taskloaf_example(totient examples/totient.cpp)
    taskloaf_example(dot_product examples/dot_product.cpp)
    taskloaf_example(diffusion examples/diffusion.cpp)
    taskloaf_example(readme examples/readme.cpp)
    taskloaf_example(fib_local examples/fib_local.cpp)
    taskloaf_example(streams examples/streams.cpp)
    
    find_package(BLAS)
    find_package(LAPACK)
    if (BLAS_FOUND AND LAPACK_FOUND)
        taskloaf_example(cholesky examples/cholesky.cpp)
        target_link_libraries(cholesky 
            ${BLAS_LIBRARIES}
            ${LAPACK_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            )
    endif()

    if (MPI_CXX_FOUND)
        taskloaf_example(fibmpi examples/fibmpi.cpp)
    endif()
endif(BUILD_EXAMPLES)
