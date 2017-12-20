import time
import numpy as np
import multiprocessing

from taskloaf.shmem import alloc_shmem, Shmem
from taskloaf.timer import Timer

def sum_shmem(sm):
    return np.sum(np.frombuffer(sm.mem))

def shmem_read(filename):
    return sum_shmem(Shmem(filename))

def test_shmem():
    with alloc_shmem('block', np.random.rand(100)) as sm:
        out = multiprocessing.Pool(1).map(shmem_read, [sm.filename])[0]
        assert(out == sum_shmem(sm))

def shmem_zeros(filename):
    np.frombuffer(Shmem(filename).mem)[:] = 0

def test_shmem_edit():
    with alloc_shmem('block', np.random.rand(100)) as sm:
        out = multiprocessing.Pool(1).map(shmem_zeros, [sm.filename])[0]
        np.testing.assert_almost_equal(np.frombuffer(sm.mem), 0.0)

def benchmark_shmem():
    A = np.random.rand(int(3e7)) - 0.5
    print('sum', np.sum(A))

    t = Timer()
    with alloc_shmem('block', memoryview(A)) as sm:
        t.report('write file')
        print('sum2', np.sum(np.frombuffer(sm.mem)))
        t.report('read and sum')

if __name__ == "__main__":
    benchmark_shmem()
