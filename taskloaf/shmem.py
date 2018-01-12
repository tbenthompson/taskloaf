import os
import mmap
import uuid
import contextlib

def get_size_from_fd(fd):
    return os.fstat(fd).st_size

def mmap_full_file(fd):
    return mmap.mmap(fd, get_size_from_fd(fd))

class Shmem:
    def __init__(self, filepath):
        self.filepath = filepath

    def __enter__(self):
        self.file = open(self.filepath, 'r+b')
        self.size = get_size_from_fd(self.file.fileno())
        self.mmap = mmap_full_file(self.file.fileno())

        # This is some crooked trickery to create a memoryview from the mmap
        # without mmap knowing about it so that the mmap can be closed without
        # tracking its references
        import numpy as np
        import ctypes
        temp_np = np.frombuffer(self.mmap, dtype = np.uint8)
        ptr = temp_np.ctypes.data
        del temp_np
        ptrc = ctypes.cast(ptr, ctypes.POINTER(ctypes.c_byte))
        new_array = np.ctypeslib.as_array(ptrc,shape=(self.size,))
        self.mem = memoryview(new_array.data.cast('B'))

        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.file.close()
        self.mmap.close()

# Note: File descriptors vs filepaths
# After creating a shared memory block, we need to be able to reference that
# block of memory and share it between processes. We could remove the file
# immediately so we don't leave cruft hanging around on the filesystem,
# and do just fine sharing the file descriptor with other processes. This
# approach relies on sending file descriptors over UNIX domain sockets. This
# sounds annoying and requires some extra code. It's probably the right
# solution so it'll happen eventually.
# The alternate approach is simply to share a filepath. This has the
# disadvantage that the file will remain on the filesystem if the process dies
# without running the corresponding clean up code.

def init_shmem_file(filepath, size):
    with open(filepath, 'w+b') as f:
        os.ftruncate(f.fileno(), size)

@contextlib.contextmanager
def alloc_shmem(size, filepath):
    assert(not os.path.exists(filepath))
    try:
        init_shmem_file(filepath, size)
        yield filepath
    finally:
        try:
            os.remove(filepath)
        except FileNotFoundError:
            pass
