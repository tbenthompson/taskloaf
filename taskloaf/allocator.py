import bisect
from collections import deque
from contextlib import ExitStack, closing
from math import log, ceil

import attr

import taskloaf.shmem

def setup_plugin(worker):
    block_root_path = worker.cfg.get('block_root_path', '/dev/shm/taskloaf_')
    local_root_path = block_root_path + str(worker.addr)
    worker.remote_shmem = worker.exit_stack.enter_context(closing(
        taskloaf.allocator.RemoteShmemRepo(block_root_path)
    ))
    block_manager = worker.exit_stack.enter_context(closing(
        taskloaf.allocator.BlockManager(local_root_path)
    ))
    worker.allocator = worker.exit_stack.enter_context(closing(
        taskloaf.allocator.ShmemAllocator(block_manager)
    ))

def get_memory_used():
    import os
    import psutil
    process = psutil.Process(os.getpid())
    return process.memory_info().rss

@attr.s
class Ptr:
    start = attr.ib()
    end = attr.ib()
    block = attr.ib()

    def deref(self):
        return self.block.shmem.mem[self.start:self.end]

    def encode_capnp(self, msg):
        msg.start = self.start
        msg.end = self.end
        msg.blockIdx = self.block.idx

    @classmethod
    def decode_capnp(self, worker, owner, msg):
        return Ptr(
            start = msg.start,
            end = msg.end,
            block = deserialize_block(worker, owner, msg.blockIdx)
        )

class MemoryBlock:
    def __init__(self, filepath, idx, shmem, _close):
        self.filepath = filepath
        self.idx = idx
        self.shmem = shmem
        self.size = shmem.size
        self._close = _close

    def close(self, *args, **kwargs):
        out = self._close(*args, **kwargs)
        del self.shmem

def alloc_memory_block(filepath, idx, size):
    with ExitStack() as es:
        es.enter_context(taskloaf.shmem.alloc_shmem(
            size, filepath
        ))
        shmem = es.enter_context(taskloaf.shmem.Shmem(filepath))
        return MemoryBlock(filepath, idx, shmem, es.pop_all().close)

def load_memory_block(filepath, idx):
    with ExitStack() as es:
        shmem = es.enter_context(taskloaf.shmem.Shmem(filepath))
        return MemoryBlock(filepath, idx, shmem, es.pop_all().close)

class RemoteShmemRepo:
    def __init__(self, block_root_path):
        self.block_root_path = block_root_path
        self.blocks = dict()

    def get_block(self, owner, block_idx):
        key = (owner, block_idx)
        if key not in self.blocks:
            filepath = self.block_root_path + str(owner) + '_' + str(block_idx)
            self.blocks[key] = load_memory_block(filepath, block_idx)
        return self.blocks[key]

    def close(self):
        for k, v in self.blocks.items():
            v.close()
        self.blocks.clear()

def deserialize_block(worker, owner, block_idx):
    return worker.remote_shmem.get_block(owner, block_idx)

class BlockManager:
    def __init__(self, root_path, page_size = taskloaf.shmem.page4kb):
        self.root_path = root_path
        self.page_size = page_size
        self.idx = 0
        self.blocks = dict()

    def get_path(self, idx):
        return self.root_path + '_' + str(idx)

    def new_block(self, size):
        idx = self.idx
        self.idx += 1
        block = alloc_memory_block(self.get_path(idx), idx, size)
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
    def __init__(self, block_manager, chunk_size, block_size = None):
        if block_size is None:
            block_size = block_manager.page_size
        self.block_manager = block_manager
        self.chunk_size = chunk_size
        self.block_size = taskloaf.shmem.roundup_to_multiple(
            block_size, block_manager.page_size
        )
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


"""
ShmemAllocator provides a typical allocator interface for claiming and freeing
memory regions in interprocess shared memory.
-- malloc(nbytes) -- returns a ptr to the new memory region
-- free(ptr) -- releases a memory region back to the allocator

Using interprocess shared memory means that the memory regions can be passed
between processes on the same machine.
"""
class ShmemAllocator:
    def __init__(self, block_manager, block_size_exponent = 17):
        self.block_manager = block_manager
        self.block_size = 2 ** block_size_exponent
        self.chunk_sizes = [2 ** i for i in range(3, block_size_exponent)]
        self.pools = [
            Pool(block_manager, s, self.block_size)
            for s in self.chunk_sizes
        ]
        self.mmap_cutoff = self.chunk_sizes[-1]

    def malloc(self, nbytes):
        if nbytes > self.mmap_cutoff:
            return self.mmap_malloc(nbytes)
        else:
            return self.pool_malloc(nbytes)

    def mmap_malloc(self, nbytes):
        full_nbytes = taskloaf.shmem.roundup_to_multiple(
            nbytes, self.block_manager.page_size
        )
        block = self.block_manager.new_block(full_nbytes)
        return Ptr(0, nbytes, block)

    def pool_malloc(self, nbytes):
        ptr = self.get_pool(nbytes).malloc()
        return Ptr(ptr.start, ptr.start + nbytes, ptr.block)

    def get_pool(self, size):
        if size == 0:
            return self.pools[0]
        pool_idx = int(ceil(log(size, 2))) - 3
        pool = self.pools[pool_idx]
        return pool

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
                return False
            for k in p.blocks:
                pool_idxs.append(k)
        for idx in self.block_manager.blocks:
            if not idx in pool_idxs:
                return False
        return True

