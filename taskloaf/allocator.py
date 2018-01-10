import os
import taskloaf.shmem

#TODO: What about freeing memory!!!!!
class Allocator:
    def __init__(self, addr, exit_stack):
        size = int(1e10)
        filename = 'taskloaf' + str(addr)
        try:
            os.remove('/dev/shm/' + filename)
        except FileNotFoundError:
            pass
        self.shmem_filepath = exit_stack.enter_context(
            taskloaf.shmem.alloc_shmem(size, filename)
        )
        self.shmem = exit_stack.enter_context(
            taskloaf.shmem.Shmem(self.shmem_filepath)
        )
        self.mem = self.shmem.mem
        self.ptr = 0
        self.addr = addr

    def alloc(self, size, alignment = 4096):
        aligned_size = size + alignment - (size % alignment)
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
        if size > 1e5:
            self.shmem.file.seek(start_ptr)
            self.shmem.file.write(in_mem)
        else:
            self.mem[start_ptr:end_ptr] = in_mem
        return start_ptr, end_ptr
