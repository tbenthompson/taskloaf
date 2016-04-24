<img src="http://tbenthompson.com/public/images/taskloaf_logo.png"/>

# taskloaf - lightweight distributed parallel futures 

taskloaf is a lightweight C++14 and python distributed futures library. taskloaf makes parallelism fun! taskloaf allows describing task graphs using verbs like "async" and "then". The tasks are then dynamically scheduled over any number of nodes using MPI or any number of cores on the local machine. taskloaf aims to be simple to use, avoiding dependencies and making compilation and usage as easy as possible. taskloaf also aims to be small, with only about 2K lines of C++.

### Check out the documentation for taskloaf
http://tbenthompson.com/taskloaf/

### Using taskloaf from python is super easy!
Just run: `pip install taskloaf`

This assumes you have a C++14 compiler available (gcc 4.8 or higher and clang 3.4 or higher should be sufficient). Check the documentation for dependencies if you have trouble. 

An example:
```python
import taskloaf as tl
from operator import add

def fib(index):
    if index < 3:
        return tl.ready(1);
    else:
        return tl.when_all(fib(index - 1), fib(index - 2)).then(add)

def done(x):
    print(x)
    return tl.shutdown()

def main():
    return fib(25).then(done)

tl.launch_mpi(main);
```

### Using taskloaf from C++ is super easy!
Installation depends on having:
* C++14-capable compiler -- gcc 4.8+ and clang 3.4+ should work. 
* CMake 2.8.12+
* MPI (optional, but required for distributed functionality)

To build:
```bash
git clone https://github.com/tbenthompson/taskloaf
cd taskloaf
mkdir build
cd build
cmake ..
make
ctest .
```

An example:
```cpp
#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib(int index) {
    if (index < 3) {
        return tl::ready(1);
    } else {
        return tl::when_all(fib(index - 1), fib(index - 2)).then(std::plus<int>());
    }
}

int main() {
    int n_cores = 2;
    tl::launch_local(n_cores, [] () {
        return fib(25).then([] (int x) {
            std::cout << "fib(25) = " << x << std::endl;
            return tl::shutdown();
        });
    });
}
```

### taskloaf is licensed under the MIT license
### build status
* master: [![Build Status](https://travis-ci.org/tbenthompson/taskloaf.svg?branch=master)](https://travis-ci.org/tbenthompson/taskloaf)
