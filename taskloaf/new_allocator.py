import bisect
from collections import deque
from contextlib import ExitStack, closing

import attr
import numpy as np

import taskloaf.shmem as shmem

@attr.s
class Ptr:
    start = attr.ib()
    end = attr.ib()
    block = attr.ib()

    def deref(self):
        return self.block.shmem.mem[self.start:self.end]


class MemoryBlock:
    def __init__(self, filepath, idx, size):
        self.filepath = filepath
        self.idx = idx
        self.size = size
        with ExitStack() as es:
            es.enter_context(shmem.alloc_shmem(
                self.size, self.filepath
            ))
            self.shmem = es.enter_context(shmem.Shmem(self.filepath))
            self.close = es.pop_all().close


class BlockManager:
    def __init__(self, root_path):
        self.root_path = root_path
        self.idx = 0
        self.blocks = dict()

    def get_path(self, idx):
        return self.root_path + '_' + str(idx)

    def new_block(self, size):
        idx = self.idx
        self.idx += 1
        block = MemoryBlock(self.get_path(idx), idx, size)
        self.blocks[idx] = block
        return block

    def free_block(self, block):
        block.close()
        del self.blocks[block.idx]

    def close(self):
        for k, v in self.blocks.items():
            v.close()
        self.blocks.clear()

    def check_location_exists(self, ptr):
        return (
            ptr.block.idx in self.blocks and
            ptr.end < self.blocks[ptr.block.idx].size and
            ptr.start <= ptr.end
        )


class PoolBlock:
    def __init__(self, block_manager, chunk_size, total_size):
        self.chunk_size = chunk_size
        self.block_manager = block_manager
        self.mem_block = block_manager.new_block(total_size)
        self.idx = self.mem_block.idx

        assert(total_size % chunk_size == 0)
        assert(chunk_size < total_size)
        self.n_chunks = total_size // chunk_size
        self.free_list = deque(range(self.n_chunks))

    def close(self):
        self.block_manager.free_block(self.mem_block)

    def chunk_ptr(self, idx):
        start = idx * self.chunk_size
        end = start + self.chunk_size
        return Ptr(start, end, self.mem_block)

    def malloc(self):
        if len(self.free_list) > 0:
            return self.chunk_ptr(self.free_list.popleft())
        return -1

    def free(self, ptr):
        idx = ptr.start // self.chunk_size
        self.free_list.appendleft(idx)

    def empty(self):
        return len(self.free_list) == self.n_chunks

    def full(self):
        return len(self.free_list) == 0


class Pool:
    def __init__(self, block_manager, chunk_size, page_size, block_size = None):
        if block_size is None:
            block_size = page_size
        self.block_manager = block_manager
        self.chunk_size = chunk_size
        self.block_size = shmem.roundup_to_multiple(block_size, page_size)
        assert(self.chunk_size <= self.block_size)

        self.blocks = dict()
        self.free_list = []

    def alloc_block(self):
        block = PoolBlock(self.block_manager, self.chunk_size, self.block_size)
        self.blocks[block.idx] = block
        self.free_list.append(block.idx)

    def malloc(self):
        if len(self.free_list) == 0:
            self.alloc_block()
        block = self.blocks[self.free_list[-1]]
        ptr = block.malloc()
        if block.full():
            self.free_list.pop()
        return ptr

    def free(self, ptr):
        block = self.blocks[ptr.block.idx]
        block_was_full = block.full()
        block.free(ptr)
        if len(self.free_list) >= 2 and block.empty():
            block.close()
            del self.blocks[block.idx]
            self.free_list.pop(bisect.bisect_left(self.free_list, block.idx))
        elif block_was_full:
            assert(block.idx not in self.free_list)
            bisect.insort(self.free_list, block.idx)

    def close(self):
        for k, v in self.blocks.items():
            v.close()

    def empty(self):
        for k, v in self.blocks.items():
            if not v.empty():
                return False
        return True


class Allocator:
    def __init__(self, block_manager, page_size, block_size_exponent = 17):
        self.page_size = page_size
        self.block_manager = block_manager
        self.block_size = 2 ** block_size_exponent
        self.chunk_sizes = [2 ** i for i in range(3, block_size_exponent)]
        self.pools = [
            Pool(block_manager, s, page_size, self.block_size)
            for s in self.chunk_sizes
        ]
        self.mmap_cutoff = self.chunk_sizes[-1]

    def mmap_malloc(self, size):
        full_size = shmem.roundup_to_multiple(size, self.page_size)
        block = self.block_manager.new_block(full_size)
        return Ptr(0, size, block)

    def get_pool(self, size):
        if size == 0:
            return self.pools[0]
        pool_idx = int(np.ceil(np.log2(size))) - 3
        pool = self.pools[pool_idx]
        return pool

    def malloc(self, size):
        if size > self.mmap_cutoff:
            return self.mmap_malloc(size)
        else:
            ptr = self.get_pool(size).malloc()
            return Ptr(ptr.start, ptr.start + size, ptr.block)

    def free(self, ptr):
        size = ptr.end - ptr.start
        if size > self.mmap_cutoff:
            self.block_manager.free_block(ptr.block)
        else:
            self.get_pool(size).free(ptr)

    def close(self):
        for p in self.pools:
            p.close()
        self.block_manager.close()

    def empty(self):
        pool_idxs = []
        for p in self.pools:
            if not p.empty():
                print(p)
                return False
            for k in p.blocks:
                pool_idxs.append(k)
        for idx in self.block_manager.blocks:
            if not idx in pool_idxs:
                print(idx)
                return False
        return True

