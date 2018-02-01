import os
import struct
import pytest
import numpy as np

from taskloaf.new_allocator import *

block_manager = BlockManager('/dev/shm/pool')

def check_ptr(ptr):
    x = np.frombuffer(ptr.deref())
    n = x.shape[0]
    x[:] = np.arange(n)
    for i in range(n):
        assert(x[i] == i)
        assert(struct.unpack('d', ptr.deref()[i * 8:(i + 1) * 8])[0] == i)

def test_pool_block():
    with closing(PoolBlock(block_manager, 2 ** 11, shmem.page4kb)) as pb:
        assert(pb.empty())
        ptr1 = pb.malloc()
        check_ptr(ptr1)
        ptr2 = pb.malloc()
        assert(ptr2 != -1)

        null = pb.malloc()
        assert(null == -1) # Out of memory!

        assert(not pb.empty())
        pb.free(ptr1)
        assert(not pb.empty())
        ptr3 = pb.malloc()
        assert(ptr3.start == ptr1.start)
        pb.free(ptr2)
        pb.free(ptr3)
        assert(pb.empty())

def test_pool():
    with closing(Pool(block_manager, 2 ** 11, shmem.page4kb)) as p:
        ptr1 = p.malloc()
        ptr2 = p.malloc()
        ptr3 = p.malloc()
        assert(ptr3 != -1)
        assert(ptr1.block == ptr2.block)
        assert(ptr1.block != ptr3.block)

def test_pool_dealloc_block():
    with closing(Pool(block_manager, 2 ** 11, shmem.page4kb)) as p:
        ptrs = [p.malloc() for i in range(6)]
        assert(len(p.blocks) == 3)
        assert(len(p.free_list) == 0)
        for ptr in ptrs[2:]:
            p.free(ptr)

        assert(len(p.free_list) == 1)
        assert(len(p.blocks) == 2)

def alloc_ptrs_then(f):
    with closing(Pool(block_manager, 2 ** 11, shmem.page4kb)) as p:
        ptrs = [p.malloc() for i in range(6)]
        f(p)

def check_no_blocks_leftover():
    assert(len(block_manager.blocks) == 0)
    for i in range(block_manager.idx):
        assert(not os.path.exists(block_manager.get_path(i)))

def test_pool_deletes_blocks():
    idxs = alloc_ptrs_then(lambda p: None)
    check_no_blocks_leftover()

def test_pool_deletes_blocks_on_exception():
    with pytest.raises(Exception):
        idxs = alloc_ptrs_then(lambda p: [][0])
    check_no_blocks_leftover()

def test_alloc_mmap():
    with closing(Allocator(block_manager, shmem.page4kb)) as a:
        ptr = a.malloc(int(1e5))
        check_ptr(ptr)
        assert(block_manager.check_location_exists(ptr))
        a.free(ptr)
        assert(not block_manager.check_location_exists(ptr))

def test_allocator_get_pool():
    with closing(Allocator(block_manager, shmem.page4kb)) as a:
        for i in range(8, 100):
            assert(a.get_pool(i).chunk_size >= i)
            assert(a.get_pool(i).chunk_size / 2 < i)

def test_alloc_pool():
    with closing(Allocator(block_manager, shmem.page4kb)) as a:
        ptrs = []
        for i in range(8, 100):
            ptrs.append(a.malloc(i))
            assert(ptrs[-1].end - ptrs[-1].start == i)
        for ptr in ptrs:
            a.free(ptr)
        assert(a.empty())

def test_speed():
    sizes = [16, 32, 40, 48, 372, 1100, 12348, 100000]
    n = 500
    ptrs = []
    from taskloaf.timer import Timer
    t = Timer()
    with closing(Allocator(block_manager, shmem.page4kb, block_size_exponent = 17)) as a:
        for i in range(n):
            if np.random.rand() > 0.5 and len(ptrs) > 0:
                idx = np.random.randint(0, len(ptrs))
                p = ptrs.pop(idx)
                # print('free', p)
                a.free(p)
            else:
                idx = np.random.randint(0, len(sizes))
                s = sizes[idx]
                # print('malloc', s)
                ptrs.append(a.malloc(s))
        for p in ptrs:
            a.free(p)
        assert(a.empty())
    # t.report('random malloc free x' + str(n))

