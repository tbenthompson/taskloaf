################################################################################
#                              Cmake config
################################################################################

cmake_minimum_required(VERSION 2.8.12)
project(taskloaf CXX C)
include(ExternalProject)

option(CMAKE_BUILD_TYPE "Build type" "Debug")
option(BUILD_LIBRARY "Build taskloaf library" ON)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_EXAMPLES "Build examples" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

################################################################################
#                              setup compiler
################################################################################

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX1Y)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
elseif (COMPILER_SUPPORTS_CXX1Y)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
    message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has only preliminary C++1y support. There may be lack of support for important features. Compilation might succeed but it would be better to use an updated C++ compiler.")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use an updated C++ compiler.")
endif() 

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # Turn on O3 and lto for Clang and gcc.
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g")
    # Clang 3.5 can't handle generating debug symbols for auto return type
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.6) 
        set(CMAKE_CXX_FLAGS_DEBUG "-O0")
    endif()
    # Lots of warnings and treat them as errors.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-mismatched-tags")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-literal-suffix")
endif()

find_package(Threads REQUIRED)

# Turn on taskloaf assertions
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DTASKLOAF_DEBUG")

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

set(TASKLOAF_SRC "src/taskloaf/remote_ref.cpp;src/taskloaf/debug.cpp;src/taskloaf/fnc_registry.cpp;src/taskloaf/local_comm.cpp;src/taskloaf/launcher.cpp;src/taskloaf/default_worker.cpp;src/taskloaf/task_collection.cpp;src/taskloaf/worker.cpp;src/taskloaf/mpi_comm.cpp;src/taskloaf/ivar.cpp;src/taskloaf/untyped_future.cpp;src/taskloaf/data.cpp")
file(GLOB TASKLOAF_HDR "src/taskloaf/*")

if (BUILD_LIBRARY)
    add_library(taskloaf SHARED ${TASKLOAF_SRC})

    target_include_directories(taskloaf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        )

    target_link_libraries(taskloaf ${CMAKE_THREAD_LIBS_INIT} boost_system)

    if (MPI_CXX_FOUND)
        target_include_directories(taskloaf SYSTEM PRIVATE ${MPI_INCLUDE_PATH})
        target_link_libraries(taskloaf ${MPI_LIBRARIES})
    endif()
    install(TARGETS taskloaf DESTINATION lib)
    install(DIRECTORY src/taskloaf/ DESTINATION include/taskloaf)
endif(BUILD_LIBRARY)

################################################################################
#                              taskloaf tests
################################################################################

macro(test_link_include name)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib)

    target_include_directories(${name} SYSTEM PRIVATE
        ${MPI_INCLUDE_PATH})
    target_link_libraries(${name} 
        taskloaf
        ${MPI_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})
endmacro()

macro(taskloaf_tests name ADDTEST)
    add_executable(${name} "$<TARGET_OBJECTS:test_runner>;tests/test_${name}.cpp")
    test_link_include(${name})
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
    taskloaf_tests(closure True)
    taskloaf_tests(data True)
    taskloaf_tests(worker True)
    taskloaf_tests(comm True)
    taskloaf_tests(future True)
    taskloaf_tests(remote_ref True)

    if (MPI_CXX_FOUND)
        add_executable(mpi "tests/test_mpi_main.cpp;tests/test_mpi.cpp")
        test_link_include(mpi)
        add_test(mpi2procs mpirun -n 2 mpi)
        add_test(mpi3procs mpirun -n 3 mpi)
        add_dependencies(tests mpi)
    endif()

    add_executable(playground "tests/playground.cpp")
    test_link_include(playground)
    add_dependencies(tests playground)
endif(BUILD_TESTS)

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
