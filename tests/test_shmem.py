import time
import numpy as np
import multiprocessing

from taskloaf.shmem import OwnedShmem, Shmem
from taskloaf.timer import Timer


def sum_shmem(sm):
    return np.sum(np.frombuffer(sm.mem))

def shmem_read(filename):
    return sum_shmem(Shmem(filename))

def test_shmem():
    with OwnedShmem('block', np.random.rand(100)) as sm:
        out = multiprocessing.Pool(1).map(shmem_read, [sm.filename])[0]
        assert(out == sum_shmem(sm))


def benchmark_shmem():
    A = np.random.rand(int(3e7)) - 0.5
    print('sum', np.sum(A))

    t = Timer()
    with OwnedShmem('block', memoryview(A)) as sm:
        t.report('write file')
        print('sum2', np.sum(np.frombuffer(sm.mem)))
        t.report('read and sum')

if __name__ == "__main__":
    benchmark_shmem()
