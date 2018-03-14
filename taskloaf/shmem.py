import os
import mmap
import uuid
import contextlib
import ctypes
import numpy as np

def get_size_from_fd(fd):
    return os.fstat(fd).st_size

def mmap_full_file(fd):
    return mmap.mmap(fd, get_size_from_fd(fd))

page4kb = 2 ** 12
page2MB = 2 ** 21
page1GB = 2 ** 30

def roundup_to_multiple(n, alignment):
    # alignment must be a power of 2
    mask = alignment - 1
    return (n + mask) & ~mask

class Shmem:
    def __init__(self, filepath, track_refs = False):
        self.filepath = filepath
        self.track_refs = track_refs

    def __enter__(self):
        self.file = open(self.filepath, 'r+b')
        self.size = get_size_from_fd(self.file.fileno())
        self.mmap = mmap_full_file(self.file.fileno())

        if not self.track_refs:
            # mmap tracks references internally. This is annoying when trying
            # to delete the mmap.  This code is some crooked trickery using
            # ctypes and grabbing the pointer from a numpy buffer to create a
            # memoryview from the mmap without mmap knowing about it so that
            # the mmap can be closed without tracking its references (this WILL
            # cause seg faults if there are existing references to the mmap
            # segment when it's deleted)
            # When using Shmem blocks through the allocator system, this is
            # fine since then taskloaf performs its own memory tracking
            temp_np = np.frombuffer(self.mmap, dtype = np.uint8)
            ptr = temp_np.ctypes.data
            del temp_np
            ptrc = ctypes.cast(ptr, ctypes.POINTER(ctypes.c_byte))
            new_array = np.ctypeslib.as_array(ptrc,shape=(self.size,))
            self.mem = memoryview(new_array.data.cast('B'))
        else:
            self.mem = memoryview(self.mmap)

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
