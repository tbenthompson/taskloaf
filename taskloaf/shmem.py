import os
import mmap
import uuid
import contextlib

def get_size_from_fd(fd):
    return os.fstat(fd).st_size

def mmap_full_file(fd):
    return mmap.mmap(fd, get_size_from_fd(fd))

class Shmem:
    def __init__(self, fd):
        self.setup(fd)

    def setup(self, fd):
        self.fd = fd
        self.mem = mmap_full_file(fd)

    def __getstate__(self):
        return self.fd

    def __setstate__(self, newstate):
        self.setup(newstate)

@contextlib.contextmanager
def alloc_shmem(obj):
    unique_filename = str(uuid.uuid4())
    filename = os.path.join('/dev/shm', unique_filename)
    try:
        with open(filename, 'w+b') as f:
            # Remove the file immediately so we don't leave cruft hanging
            # around on the filesystem. We'll do just fine with only a
            # file descriptor
            os.remove(filename)

            if type(obj) is int:
                # Create a sparse file of the proper size.
                f.seek(obj - 1)
                f.write(bytes(1))
                f.seek(0)
            else:
                f.write(obj)
                f.seek(0)
            yield Shmem(f.fileno())
    finally:
        pass
