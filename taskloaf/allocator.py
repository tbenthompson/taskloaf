import os
import taskloaf.shmem
import math
import bisect

shm_root = '/dev/shm'
# shm_root = '/mnt/hugepages'
def get_shmem_filepath(addr):
    return os.path.join(shm_root, 'taskloaf') + str(addr)

class RemoteShmemRepo:
    def __init__(self, exit_stack):
        self.exit_stack = exit_stack
        self.remotes = dict()

    def get(self, dref):
        if dref.owner not in self.remotes:
            self.remotes[dref.owner] = self.exit_stack.enter_context(
                taskloaf.shmem.Shmem(get_shmem_filepath(dref.owner))
            )
        shmem = self.remotes[dref.owner]
        return dref.shmem_ptr.dereference(shmem.mem)

page4kb = 2 ** 12
page2MB = 2 ** 21
page1GB = 2 ** 30
def roundup(n, alignment):
    # alignment must be a power of 2
    mask = alignment - 1
    return (n + mask) & ~mask

class Allocator:
    def __init__(self, addr, exit_stack, pagesize = page2MB):
        size = roundup(int(4e9), pagesize)
        filepath = get_shmem_filepath(addr)
        try:
            os.remove(filepath)
        except FileNotFoundError:
            pass
        self.shmem_filepath = exit_stack.enter_context(
            taskloaf.shmem.alloc_shmem(size, filepath)
        )
        self.shmem = exit_stack.enter_context(
            taskloaf.shmem.Shmem(self.shmem_filepath)
        )
        # self.mem = self.shmem.mem
        self.ptr = 0
        self.addr = addr
        # TODO: Ensure that the original self.ptr is aligned to self.alignment
        # 32 byte alignment could be useful for AVX
        self.alignment = 16

    @property
    def mem(self):
        return self.shmem.mem

    def alloc(self, size):
        aligned_size = size + self.alignment - (size % self.alignment)
        start_ptr = self.ptr
        end_ptr = self.ptr + size
        self.ptr += aligned_size
        # print(self.addr, old_ptr, next_ptr)
        return start_ptr, end_ptr

    def store(self, in_mem):
        size = in_mem.nbytes
        start_ptr, end_ptr = self.alloc(size)
        # print(self.addr, 'storing', size)
        if end_ptr > len(self.mem):
            raise Exception('Out of memory!')
        self.mem[start_ptr:end_ptr] = in_mem
        return start_ptr, end_ptr

import contextlib

class ShmemArenaBuilder:
    def __init__(self, pagesize, rootfilepath):
        self.pagesize = pagesize
        self.rootfilepath = rootfilepath

    def __enter__(self):
        self.exit_stack = contextlib.ExitStack()

    def __exit__(self, *args):
        self.exit_stack.close()

    def __call__(self, size):
        pass

# Adapted from multiprocessing.heap
#TODO: Never frees/deletes an arena (build_arena should return a third value --
# a deleter function)
class NewAllocator:
    def __init__(self, alignment, build_arena):
        self._alignment = alignment
        self._size = 0
        self._lengths = []
        self._len_to_seq = {}
        self._start_to_block = {}
        self._stop_to_block = {}
        self._allocated_blocks = set()
        self._arenas = []
        self._build_arena = build_arena

    def _malloc(self, size):
        # returns a large enough block -- it might be much larger
        i = bisect.bisect_left(self._lengths, size)
        if i == len(self._lengths):
            prelim_length = max(self._size, size)
            arena = self._build_arena(prelim_length)
            final_length = arena[0]
            #TODO: logging
            print('allocated a new mmap of length', final_length)
            self._size += final_length
            self._arenas.append(arena)
            return (arena, 0, final_length)
        else:
            length = self._lengths[i]
            seq = self._len_to_seq[length]
            block = seq.pop()
            if not seq:
                del self._len_to_seq[length], self._lengths[i]

            (arena, start, stop) = block
            del self._start_to_block[(arena, start)]
            del self._stop_to_block[(arena, stop)]
            return block

    def _free(self, block):
        # free location and try to merge with neighbours
        (arena, start, stop) = block

        try:
            prev_block = self._stop_to_block[(arena, start)]
        except KeyError:
            pass
        else:
            start, _ = self._absorb(prev_block)

        try:
            next_block = self._start_to_block[(arena, stop)]
        except KeyError:
            pass
        else:
            _, stop = self._absorb(next_block)

        block = (arena, start, stop)
        length = stop - start

        try:
            self._len_to_seq[length].append(block)
        except KeyError:
            self._len_to_seq[length] = [block]
            bisect.insort(self._lengths, length)

        self._start_to_block[(arena, start)] = block
        self._stop_to_block[(arena, stop)] = block

    def _absorb(self, block):
        # deregister this block so it can be merged with a neighbour
        (arena, start, stop) = block
        del self._start_to_block[(arena, start)]
        del self._stop_to_block[(arena, stop)]

        length = stop - start
        seq = self._len_to_seq[length]
        seq.remove(block)
        if not seq:
            del self._len_to_seq[length]
            self._lengths.remove(length)

        return start, stop

    def free(self, block):
        # free a block returned by malloc()
        self._allocated_blocks.remove(block)
        self._free(block)

    def malloc(self, size):
        # return a block of right size (possibly rounded up)
        if size < 0:
            raise ValueError("Size {0:n} out of range".format(size))

        size = roundup(max(size,1), self._alignment)
        (arena, start, stop) = self._malloc(size)
        new_stop = start + size
        if new_stop < stop:
            self._free((arena, new_stop, stop))
        block = (arena, start, new_stop)
        self._allocated_blocks.add(block)
        return block
