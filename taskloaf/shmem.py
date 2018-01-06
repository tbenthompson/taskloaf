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
        self.mem = mmap_full_file(self.file.fileno())
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.file.close()

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

def init_shmem_file(filepath, obj):
    with open(filepath, 'w+b') as f:
        if type(obj) is int:
            # Create a sparse file of the proper size.
            f.seek(obj - 1)
            f.write(bytes(1))
            f.seek(0)
        else:
            f.write(obj)
            f.seek(0)

@contextlib.contextmanager
def alloc_shmem(obj, filename = None):
    if filename is None:
        filename = str(uuid.uuid4())
    filepath = os.path.join('/dev/shm', filename)
    assert(not os.path.exists(filepath))
    try:
        init_shmem_file(filepath, obj)
        yield filepath
    finally:
        os.remove(filepath)
