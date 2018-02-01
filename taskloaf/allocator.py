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
