import os
import mmap
import contextlib

class Shmem:
    def __init__(self, filename):
        self.filename = filename
        self.fd = os.open(self.filename, os.O_RDWR)
        self.mem = mmap.mmap(self.fd, os.path.getsize(self.filename))

    def __del__(self):
        os.close(self.fd)

@contextlib.contextmanager
def alloc_shmem(name, writeable):
    filename = os.path.join('/dev/shm', name)
    try:
        with open(filename, 'wb') as f:
            f.write(writeable)
        del writeable
        yield Shmem(filename)
    finally:
        os.remove(filename)
