# Dependencies (Python and C++)
First check that you have the dependencies:

* git
* CMake 2.8.12 or higher. This can be checked with `cmake --version`.
* A C++14 compiler. taskloaf has been tested with g++ 4.8 and higher and with clang 3.4 and higher. For g++ check the version with `g++ -v`. For clang check the version with `clang++ -v`. If you are using another compiler, then I'd love to hear from you about whether taskloaf works with your compiler.
* An MPI installation is necessary for running in a distributed setting. CMake will automatically detect whether you have an MPI installation available. The FindMPI CMake module is used. To specify a specific MPI compiler, you can replace `cmake ..` below with `cmake -DMPI_CXX_COMPILER=compilername ..`. For more information, take a look at [https://cmake.org/cmake/help/v3.0/module/FindMPI.html](https://cmake.org/cmake/help/v3.0/module/FindMPI.html).
 
On Ubuntu 14.04 and newer, all of the above can be installed with:
```bash
sudo apt-get install libopenmpi-dev openmpi-bin cmake build-essential git 
```

# Installation for Python

Under most circumstances, installing taskloaf for python should just be:
```bash
pip install taskloaf
```

# Installation for C++

Next, download taskloak:

```bash
git clone git@github.com:tbenthompson/taskloaf.git
```

Create a build directory and run cmake:

```bash
mkdir -p taskloaf/build
cd taskloaf/build
cmake ..
```

Finally, start the compilation (this uses 2 compilation threads, use as many as you can!)
```bash
make -j2
```

To check whether the compilation was successful, run the tests:
```bash
make test
```
