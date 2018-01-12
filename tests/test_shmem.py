import os
import time
import numpy as np
import multiprocessing

from taskloaf.shmem import alloc_shmem, Shmem
from taskloaf.timer import Timer

def sum_shmem(filepath):
    with Shmem(filepath) as sm:
        return np.sum(np.frombuffer(sm.mem))

def test_shmem():
    A = np.random.rand(100)
    with alloc_shmem(A.nbytes, 'test') as filepath:
        with Shmem(filepath) as sm:
            sm.mem[:] = A.data.cast('B')
            out = multiprocessing.Pool(1).map(sum_shmem, [sm.filepath])[0]
            assert(out == sum_shmem(sm.filepath))

def shmem_zeros(filepath):
    with Shmem(filepath) as sm:
        np.frombuffer(sm.mem)[:] = 0

def test_shmem_edit():
    A = np.random.rand(100)
    with alloc_shmem(A.nbytes, 'test') as filepath:
        with Shmem(filepath) as sm:
            sm.mem[:] = A.data.cast('B')
            out = multiprocessing.Pool(1).map(shmem_zeros, [sm.filepath])[0]
            np.testing.assert_almost_equal(np.frombuffer(sm.mem), 0.0)

"""
There's some complexity in this benchmark. The performance of allocating mmap
is pretty much identical to a alloc+memcpy if you can write the relevant data
to the mmap-ed region before mmap-ing it. However, if you fill the memory
through the pointer returned by mmap, mmap will zero the memory region first.

An approach that can avoid this problem is to write directly to the file
associated with the mmap-ed region. Because the memory is not actually
allocated until it's used, this will avoid the time cost of mmap zeroing the
memory

Also, note that the lazy nature of python garbage collection means that munmap
is called at unexpected times and cause some problems with benchmarking.

LATER: i THINK THAT ALL THESE WOES WERE CAUSED ALMOST ENTIRELY BY THE LACK OF
USING HUGE PAGES
"""
def benchmark_shmem():
    t = Timer()
    A = np.random.rand(int(1e8)) - 0.5
    t.report('build A')
    print('sum', np.sum(A))
    t.report('sum')
    b = A.copy()
    t.report('baseline copy')

    with alloc_shmem(A) as sm:
        t.report('alloc fillled shmem')
        del sm.mem

    t.restart()
    with alloc_shmem(A.nbytes) as sm:
        f = open(sm.fd, 'r+b', closefd=False)
        f.seek(0)
        f.write(A)
        f.close()
        t.report('alloc empty, write to file')
        del sm.mem

    t.restart()
    with alloc_shmem(A.nbytes) as sm:
        np.frombuffer(sm.mem)[:] = A
        t.report('alloc empty shmem and fill')
        del sm.mem


    with alloc_shmem(A) as sm:
        t.restart()
        print('sum2', np.sum(np.frombuffer(sm.mem)))
        t.report('read and sum')
        del sm.mem


if __name__ == "__main__":
    benchmark_shmem()
