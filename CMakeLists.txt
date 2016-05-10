################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX C)
set(CMAKE_BUILD_TYPE Release)
include(ExternalProject)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-mismatched-tags")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-literal-suffix")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

find_package(Threads REQUIRED)

# Turn on taskloaf assertions
add_definitions(-DTLASSERT)

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

set(TASKLOAF_SRC "src/taskloaf/ivar.cpp;src/taskloaf/worker.cpp;src/taskloaf/task_collection.cpp;src/taskloaf/ivar_tracker.cpp;src/taskloaf/id.cpp;src/taskloaf/ring.cpp;src/taskloaf/ref_counting.cpp;src/taskloaf/local_comm.cpp;src/taskloaf/serializing_comm.cpp;src/taskloaf/comm.cpp;src/taskloaf/launcher.cpp;src/taskloaf/mpi_comm.cpp;src/taskloaf/tlassert.cpp;src/taskloaf/fnc.cpp;src/taskloaf/data.cpp")
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
endif(BUILD_LIBRARY)

################################################################################
#                              taskloaf tests
################################################################################

macro(taskloaf_tests name files)
    add_executable(${name} ${files})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib)

    target_include_directories(${name} SYSTEM PRIVATE
        ${MPI_INCLUDE_PATH})
    add_dependencies(${name} taskloaf)
    target_link_libraries(${name} 
        taskloaf
        ${MPI_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
endmacro()

if(BUILD_TESTS)
    taskloaf_tests(run_tests
        "tests/test_main.cpp;tests/test_future.cpp;tests/test_future2.cpp;tests/test_fnc.cpp;tests/test_data.cpp;tests/test_worker.cpp;tests/test_id.cpp;tests/test_ring.cpp;tests/test_comm.cpp;tests/test_ref_counting.cpp"
    )

    enable_testing()
    add_test(main-tests run_tests)

    if (MPI_CXX_FOUND)
        taskloaf_tests(mpi_tests "tests/test_mpi.cpp")
        add_test(mpi-tests mpirun -n 2 mpi_tests)
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
    add_dependencies(${name} taskloaf)
    target_link_libraries(${name} 
        taskloaf
        ${CMAKE_THREAD_LIBS_INIT})
endmacro()

#if (BUILD_EXAMPLES)
#    taskloaf_example(totient examples/totient.cpp)
#    taskloaf_example(dot_product examples/dot_product.cpp)
#    taskloaf_example(diffusion examples/diffusion.cpp)
#    taskloaf_example(readme examples/readme.cpp)
#    
#    find_package(BLAS)
#    find_package(LAPACK)
#    if (BLAS_FOUND AND LAPACK_FOUND)
#        taskloaf_example(cholesky examples/cholesky.cpp)
#        target_link_libraries(cholesky 
#            ${BLAS_LIBRARIES}
#            ${LAPACK_LIBRARIES}
#            ${CMAKE_THREAD_LIBS_INIT}
#            )
#    endif()
#
#    if (MPI_CXX_FOUND)
#        taskloaf_example(fibmpi examples/fibmpi.cpp)
#    endif()
#endif(BUILD_EXAMPLES)
